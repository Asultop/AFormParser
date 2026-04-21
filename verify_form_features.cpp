#include <QCoreApplication>
#include <QTextStream>

#include "AFormParser.hpp"

namespace {

bool check(bool cond, const QString &msg, QTextStream &qout, QTextStream &qerr)
{
    if (cond) {
        qout << "[ok] " << msg << "\n";
        return true;
    }
    qerr << "[fail] " << msg << "\n";
    return false;
}

QString buildFixtureForm()
{
    return QString::fromUtf8(R"FORM(
@Version: 1.0
@Author: Verify
@Description: Verify Dump/toCFG/Traversal/Scripts

Form{
    .Id = "VerifyForm"
    Group{
        .Title = "验证分组"

        KeyBind{
            .Id = "Key_Main"
            .Enabled = true
            .Description = "主键"
            .SubDescription = "绑定主键"
            .Command = "+attack"
            .Bind("mouse1")
        }

        LineField{
            .Id = "Line_Main"
            .Enabled = true
            .Description = "参数行"
            .SubDescription = "脚本表达式"
            .Args{
                .Arg{
                    .Id = "arg1"
                    .Description = "参数1"
                    .Value = "AA"
                }
                .Arg{
                    .Id = "arg2"
                    .Description = "参数2"
                    .Value = "BB"
                }
            }
            .Expression = $JoinArgs(arg1,arg2)
        }

        OptionField{
            .Id = "Opt_Main"
            .Enabled = true
            .Description = "模式"
            .SubDescription = "选择模式"
            Options{
                Option{
                    .Id = "Mode_1"
                    .Description = "模式一"
                    .Command = "echo mode1"
                }
                Option{
                    .Id = "Mode_2"
                    .Description = "模式二"
                    .Command = "echo mode2"
                }
            }
            .Selected = "Mode_2"
        }
    }
}

Scripts{
    function JoinArgs(a,b)
        return "echo " .. a .. "_" .. b
    end
}
)FORM").trimmed();
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream qout(stdout);
    QTextStream qerr(stderr);

    bool okAll = true;

    const QString formText = buildFixtureForm();

    AFormParser::ParseError parseError;
    auto doc = AFormParser::Document::from(formText, &parseError);
    okAll &= check(doc != nullptr,
                   QStringLiteral("Document::from() 可解析 Form"),
                   qout,
                   qerr);
    if (!doc) {
        qerr << "[detail] line=" << parseError.line
             << " col=" << parseError.column
             << " msg=" << parseError.message << "\n";
        return 1;
    }

    okAll &= check(doc->form != nullptr,
                   QStringLiteral("Document 含 Form 节点"),
                   qout,
                   qerr);
    okAll &= check(doc->scripts != nullptr,
                   QStringLiteral("Document 含 Scripts 节点"),
                   qout,
                   qerr);

    const QString dumped = doc->dump();
    okAll &= check(!dumped.trimmed().isEmpty(),
                   QStringLiteral("Document::dump() 输出非空"),
                   qout,
                   qerr);
    okAll &= check(dumped.contains(QStringLiteral("Form{")) && dumped.contains(QStringLiteral("Scripts{")),
                   QStringLiteral("dump 保留 Form 与 Scripts 结构"),
                   qout,
                   qerr);

    AFormParser::ParseError reparseError;
    auto reparsed = AFormParser::Document::from(dumped, &reparseError);
    okAll &= check(reparsed != nullptr,
                   QStringLiteral("dump 结果可再次 parse"),
                   qout,
                   qerr);
    if (!reparsed) {
        qerr << "[detail] reparse line=" << reparseError.line
             << " col=" << reparseError.column
             << " msg=" << reparseError.message << "\n";
    }

    const auto nodes = doc->allNodes();
    okAll &= check(!nodes.isEmpty() && nodes.size() >= 8,
                   QStringLiteral("Document::allNodes() 可遍历节点"),
                   qout,
                   qerr);

    const auto keyNode = doc->findById(QStringLiteral("Key_Main"));
    okAll &= check(keyNode != nullptr && keyNode->toKeyBind() != nullptr,
                   QStringLiteral("Document::findById() 可定位 KeyBind"),
                   qout,
                   qerr);

    const auto argNode = doc->findById(QStringLiteral("arg1"));
    okAll &= check(argNode != nullptr && argNode->toArg() != nullptr,
                   QStringLiteral("Document::findById() 可定位 Arg"),
                   qout,
                   qerr);

    const QString cfg = doc->toCFG();
    okAll &= check(!cfg.trimmed().isEmpty(),
                   QStringLiteral("Document::toCFG() 输出非空"),
                   qout,
                   qerr);

    okAll &= check(cfg.contains(QStringLiteral("bind mouse1 +attack")),
                   QStringLiteral("toCFG 含 KeyBind 导出"),
                   qout,
                   qerr);

    okAll &= check(cfg.contains(QStringLiteral("echo mode2")),
                   QStringLiteral("toCFG 含 OptionField 选中项导出"),
                   qout,
                   qerr);

    // Script 功能验证：期望 JoinArgs 被执行，输出 echo AA_BB
    okAll &= check(cfg.contains(QStringLiteral("echo AA_BB")),
                   QStringLiteral("toCFG 成功执行 Scripts 函数"),
                   qout,
                   qerr);

    const QString processArgsForm = QString::fromUtf8(R"FORM(
Form{
    .Id = "ProcessArgsForm"
    Group{
        .Title = "ProcessArgs 回归"
        LineField{
            .Id = "Line_Proc"
            .Enabled = true
            .Description = "ProcessArgs"
            .SubDescription = "回归"
            .Args{
                .Arg{
                    .Id = "id1"
                    .Description = "1"
                    .Value = "1"
                }
                .Arg{
                    .Id = "id2"
                    .Description = "2"
                    .Value = "2"
                }
                .Arg{
                    .Id = "id3"
                    .Description = "3"
                    .Value = "3"
                }
            }
            .Expression = "echo" + $ProcessArgs(id1,id2,id3)
        }
    }
}

Scripts{
    function ProcessArgs(arg1,arg2,arg3)
        local result = ""
        if arg3 ~= "" then
            result = result .. " " .. arg3
        end
        if arg2 ~= "" then
            result = result .. " " .. arg2
        end
        if arg1 ~= "" then
            result = result .. " " .. arg1
        end
        return result
    end
}
)FORM").trimmed();

    AFormParser::ParseError processArgsErr;
    auto processArgsDoc = AFormParser::Document::from(processArgsForm, &processArgsErr);
    okAll &= check(processArgsDoc != nullptr,
                   QStringLiteral("ProcessArgs 回归 Form 可解析"),
                   qout,
                   qerr);
    if (!processArgsDoc) {
        qerr << "[detail] processArgs line=" << processArgsErr.line
             << " col=" << processArgsErr.column
             << " msg=" << processArgsErr.message << "\n";
    } else {
        const QString processCfg = processArgsDoc->toCFG();
        okAll &= check(!processCfg.contains(QStringLiteral("ProcessArgs("))
                           && processCfg.contains(QStringLiteral("echo")),
                       QStringLiteral("LineField 中 $ProcessArgs 不以字面量输出"),
                       qout,
                       qerr);
        if (processCfg.contains(QStringLiteral("ProcessArgs("))) {
            qerr << "[detail] processCfg=\n" << processCfg << "\n";
        }
    }

    const QString badScriptText = QString::fromUtf8(R"FORM(
Form{
    .Id = "BadScript"
    Group{
        .Title = "Bad"
    }
}
Scripts{
    function BadFn(a,b)
        return "x" ..
    end
}
)FORM").trimmed();

    AFormParser::ParseError badScriptErr;
    auto badScriptDoc = AFormParser::Document::from(badScriptText, &badScriptErr);
    okAll &= check(badScriptDoc == nullptr
                       && badScriptErr.line > 0
                       && badScriptErr.column > 0
                       && badScriptErr.message.contains(QStringLiteral("Scripts")),
                   QStringLiteral("Scripts 语法错误可在 parse 阶段定位行列"),
                   qout,
                   qerr);

    const QString badKvText = QString::fromUtf8(R"FORM(
Form{
    .Id = "BadKv"
    Group{
        .Title = "Bad"
        TextField{
            .Id = "T1"
            .Command =
        }
    }
}
)FORM").trimmed();

    AFormParser::ParseError badKvErr;
    auto badKvDoc = AFormParser::Document::from(badKvText, &badKvErr);
    okAll &= check(badKvDoc == nullptr
                       && badKvErr.line > 0
                       && badKvErr.column > 0
                       && badKvErr.message.contains(QStringLiteral("键值对")),
                   QStringLiteral("键值对不匹配可精准报错"),
                   qout,
                   qerr);

    const QString badTokenText = QString::fromUtf8(R"FORM(
Form{
    .Id = "BadToken" }
}
)FORM").trimmed();

    AFormParser::ParseError badTokenErr;
    auto badTokenDoc = AFormParser::Document::from(badTokenText, &badTokenErr);
    okAll &= check(badTokenDoc == nullptr
                       && badTokenErr.line > 0
                       && badTokenErr.column > 0
                       && badTokenErr.message.contains(QStringLiteral("Token")),
                   QStringLiteral("Token 不匹配可精准报错"),
                   qout,
                   qerr);

    if (!okAll) {
        qerr << "\n[summary] verify_form_features FAILED\n";
        qerr << "[cfg]\n" << cfg << "\n";
        return 2;
    }

    qout << "\n[summary] verify_form_features PASSED\n";
    qout << "[cfg]\n" << cfg << "\n";
    return 0;
}
