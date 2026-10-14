//
//  Header.h
//  dommyTool
//
//  Created by yuzo on 2017/08/28.
//



#pragma once

#include "common/utility/DSeExprVariables.h"

#include <SeExpression.h>
#include <SeExprFunc.h>
#include <SeVec3d.h>
#include "SeExprNode.h"

class DCurveExpr : public SeExpression {
public:
    DCurveExpr(const std::string& expr) : SeExpression(expr) {}
    
    void setParam(double p_input) { param.val = p_input; }
    void setLength(double p_input) { len.val = p_input; }
    
private:
  using SimpleVar = dommy::expression::ScalarVariable;

  mutable SimpleVar param;
  mutable SimpleVar len;

  SeExprVarRef* resolveVar(const std::string& name) const {
      if (name == "param")
          return &param;
      if (name == "len")
          return &len;
      return 0;
  }
};
