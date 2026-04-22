#pragma once

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <QMap>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>

namespace AFormParser {

class LuaRuntime {
public:
    using LuaFunction = std::function<QString(QStringList)>;

    LuaRuntime();
    ~LuaRuntime();

    bool loadScript(const QString &script, QString *error = nullptr);
    bool executeFunction(const QString &fnName, const QStringList &args, QString *result = nullptr, QString *error = nullptr);

    void registerFunction(const QString &name, LuaFunction func);
    void unregisterFunction(const QString &name);

    void registerGlobalVariable(const QString &name, const QString &value);
    void unregisterGlobalVariable(const QString &name);
    void setGlobalVariables(const QMap<QString, QString> &vars);

    void clear();

private:
    struct LuaDeleter {
        void operator()(lua_State *L) const;
    };

    std::unique_ptr<lua_State, LuaDeleter> L_;
    QMap<QString, LuaFunction> registeredFunctions_;
    QMap<QString, QString> globalVariables_;

    static int functionBridge(lua_State *L);
};

} // namespace AFormParser
