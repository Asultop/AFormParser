#include "AFormParser/LuaRuntime.hpp"

#include <QString>

extern "C" {
typedef int (*lua_CFunction)(lua_State *L);
}

namespace AFormParser {

void LuaRuntime::LuaDeleter::operator()(lua_State *L) const {
    if (L) {
        lua_close(L);
    }
}

static QMap<QString, std::function<QString(QStringList)>> *g_functionRegistry = nullptr;
static QMap<QString, QString> *g_globalVariables = nullptr;

int LuaRuntime::functionBridge(lua_State *L)
{
    if (!g_functionRegistry) {
        lua_pushstring(L, "Error: function registry not initialized");
        return 1;
    }

    const char *name = lua_tostring(L, lua_upvalueindex(1));
    if (!name) {
        lua_pushstring(L, "Error: no function name in upvalue");
        return 1;
    }

    auto it = g_functionRegistry->find(name);
    if (it == g_functionRegistry->end()) {
        lua_pushstring(L, QString("Error: function '%1' not found").arg(name).toUtf8().constData());
        return 1;
    }

    int n = lua_gettop(L);
    QStringList args;
    for (int i = 1; i <= n; ++i) {
        if (lua_isstring(L, i)) {
            args.append(QString::fromUtf8(lua_tostring(L, i)));
        } else if (lua_isnumber(L, i)) {
            args.append(QString::number(lua_tonumber(L, i)));
        } else if (lua_isboolean(L, i)) {
            args.append(lua_toboolean(L, i) ? "true" : "false");
        } else {
            args.append("nil");
        }
    }

    QString result = it.value()(args);
    lua_pushstring(L, result.toUtf8().constData());
    return 1;
}

LuaRuntime::LuaRuntime()
{
    L_.reset(luaL_newstate());
    if (L_) {
        luaL_openlibs(L_.get());
    }

    if (!g_functionRegistry) {
        g_functionRegistry = new QMap<QString, LuaFunction>();
    }
    if (!g_globalVariables) {
        g_globalVariables = new QMap<QString, QString>();
    }
}

LuaRuntime::~LuaRuntime() = default;

void LuaRuntime::clear()
{
    for (auto it = registeredFunctions_.constBegin(); it != registeredFunctions_.constEnd(); ++it) {
        g_functionRegistry->remove(it.key());
        lua_pushnil(L_.get());
        lua_setglobal(L_.get(), it.key().toUtf8().constData());
    }
    registeredFunctions_.clear();

    for (auto it = globalVariables_.constBegin(); it != globalVariables_.constEnd(); ++it) {
        g_globalVariables->remove(it.key());
        lua_pushnil(L_.get());
        lua_setglobal(L_.get(), it.key().toUtf8().constData());
    }
    globalVariables_.clear();
}

bool LuaRuntime::loadScript(const QString &script, QString *error)
{
    if (!L_) {
        if (error) {
            *error = QStringLiteral("Lua state not initialized");
        }
        return false;
    }

    QString processedScript = script;
    for (auto it = g_globalVariables->constBegin(); it != g_globalVariables->constEnd(); ++it) {
        if (it.key().startsWith("__")) {
            QString pattern = it.key();
            QString value = it.value();
            processedScript.replace(pattern, value);
        }
    }

    int loadResult = luaL_loadstring(L_.get(), processedScript.toUtf8().constData());
    if (loadResult != LUA_OK) {
        if (error) {
            *error = lua_tostring(L_.get(), -1);
        }
        lua_pop(L_.get(), 1);
        return false;
    }

    int execResult = lua_pcall(L_.get(), 0, LUA_MULTRET, 0);
    if (execResult != LUA_OK) {
        if (error) {
            *error = lua_tostring(L_.get(), -1);
        }
        lua_pop(L_.get(), 1);
        return false;
    }

    return true;
}

bool LuaRuntime::executeFunction(const QString &fnName, const QStringList &args, QString *result, QString *error)
{
    if (!L_) {
        if (error) {
            *error = QStringLiteral("Lua state not initialized");
        }
        return false;
    }

    int stackBefore = lua_gettop(L_.get());
    lua_getglobal(L_.get(), fnName.toUtf8().constData());
    if (!lua_isfunction(L_.get(), -1)) {
        lua_pop(L_.get(), 1);
        if (error) {
            *error = QStringLiteral("Function '") + fnName + QStringLiteral("' not found");
        }
        return false;
    }

    for (const QString &arg : args) {
        lua_pushstring(L_.get(), arg.toUtf8().constData());
    }

    int callResult = lua_pcall(L_.get(), args.size(), LUA_MULTRET, 0);
    if (callResult != LUA_OK) {
        if (error) {
            *error = lua_tostring(L_.get(), -1);
        }
        lua_pop(L_.get(), 1);
        return false;
    }

    if (result) {
        int numResults = lua_gettop(L_.get()) - stackBefore;
        if (numResults > 0) {
            const char *res = lua_tostring(L_.get(), -numResults);
            *result = QString::fromUtf8(res ? res : "");
        }
    }

    lua_pop(L_.get(), lua_gettop(L_.get()) - stackBefore);
    return true;
}

void LuaRuntime::registerFunction(const QString &name, LuaFunction func)
{
    registeredFunctions_[name] = func;
    (*g_functionRegistry)[name] = func;

    lua_pushstring(L_.get(), name.toUtf8().constData());
    lua_pushcclosure(L_.get(), &functionBridge, 1);
    lua_setglobal(L_.get(), name.toUtf8().constData());
}

void LuaRuntime::unregisterFunction(const QString &name)
{
    registeredFunctions_.remove(name);
    g_functionRegistry->remove(name);
    lua_pushnil(L_.get());
    lua_setglobal(L_.get(), name.toUtf8().constData());
}

void LuaRuntime::registerGlobalVariable(const QString &name, const QString &value)
{
    globalVariables_[name] = value;
    (*g_globalVariables)[name] = value;

    lua_pushstring(L_.get(), value.toUtf8().constData());
    lua_setglobal(L_.get(), name.toUtf8().constData());
}

void LuaRuntime::unregisterGlobalVariable(const QString &name)
{
    globalVariables_.remove(name);
    g_globalVariables->remove(name);
    lua_pushnil(L_.get());
    lua_setglobal(L_.get(), name.toUtf8().constData());
}

void LuaRuntime::setGlobalVariables(const QMap<QString, QString> &vars)
{
    for (auto it = vars.constBegin(); it != vars.constEnd(); ++it) {
        registerGlobalVariable(it.key(), it.value());
    }
}

} // namespace AFormParser
