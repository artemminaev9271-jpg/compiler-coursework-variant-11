#include "ast.hpp"

namespace mc {

const char *typeName(TypeKind type) {
    switch (type) {
    case TypeKind::Int:
        return "int";
    case TypeKind::Bool:
        return "bool";
    }
    return "<unknown>";
}

} // namespace mc
