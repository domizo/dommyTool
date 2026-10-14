#include <SeExprFunc.h>
#include <SeExprNode.h>
#include <SeExpression.h>
#include <SeVec3d.h>

#include <string>

class LegacyExpression : public SeExpression
{
public:
    explicit LegacyExpression(const std::string& source)
        : SeExpression(source)
    {
    }

    SeExprVarRef* resolveVar(const std::string& /*name*/) const override
    {
        return nullptr;
    }
};

int main()
{
    LegacyExpression expression("[1, 1, 1]");
    SeVec3d value(0.0, 0.0, 0.0);
    (void)expression;
    (void)value;
    return 0;
}
