#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "ast.hpp"
#include "codegen.hpp"
#include "sema.hpp"

extern FILE *yyin;
extern int yyparse();
extern int yylineno;

namespace mc {
extern std::unique_ptr<Program> g_parsed_program;
extern std::vector<std::string> g_parse_errors;
}

namespace {

struct Options {
    std::string input;
    std::string objectOutput = "output.o";
    std::string irOutput;
};

void printUsage(const char *program) {
    std::cerr << "Usage: " << program << " input.mc -o output.o [--emit-ir output.ll]\n";
}

bool parseArgs(int argc, char **argv, Options &options) {
    if (argc < 2) {
        return false;
    }

    options.input = argv[1];
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o") {
            if (++i >= argc) {
                std::cerr << "-o requires a file name\n";
                return false;
            }
            options.objectOutput = argv[i];
        } else if (arg == "--emit-ir") {
            if (++i >= argc) {
                std::cerr << "--emit-ir requires a file name\n";
                return false;
            }
            options.irOutput = argv[i];
        } else {
            std::cerr << "unknown argument: " << arg << "\n";
            return false;
        }
    }

    return !options.input.empty();
}

} // namespace

int main(int argc, char **argv) {
    Options options;
    if (!parseArgs(argc, argv, options)) {
        printUsage(argv[0]);
        return 1;
    }

    yyin = std::fopen(options.input.c_str(), "r");
    if (!yyin) {
        std::perror(options.input.c_str());
        return 1;
    }

    yylineno = 1;
    mc::g_parse_errors.clear();
    mc::g_parsed_program.reset();

    int parseResult = yyparse();
    std::fclose(yyin);

    if (parseResult != 0 || !mc::g_parse_errors.empty() || !mc::g_parsed_program) {
        for (const auto &err : mc::g_parse_errors) {
            std::cerr << "parse error: " << err << "\n";
        }
        if (mc::g_parse_errors.empty()) {
            std::cerr << "parse error: failed to parse input\n";
        }
        return 1;
    }

    mc::SemanticAnalyzer sema;
    if (!sema.analyze(*mc::g_parsed_program)) {
        for (const auto &err : sema.errors()) {
            std::cerr << "semantic error: " << err << "\n";
        }
        return 1;
    }

    mc::CodeGenerator codegen;
    if (!codegen.generate(*mc::g_parsed_program)) {
        std::cerr << "codegen error: " << codegen.error() << "\n";
        return 1;
    }

    if (!options.irOutput.empty() && !codegen.emitIR(options.irOutput)) {
        std::cerr << "IR output error: " << codegen.error() << "\n";
        return 1;
    }

    if (!codegen.emitObject(options.objectOutput)) {
        std::cerr << "object output error: " << codegen.error() << "\n";
        return 1;
    }

    return 0;
}
