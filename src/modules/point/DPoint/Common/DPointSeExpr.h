//
//  DPointSeExpr.h
//  dommyTool
//
//  Created by yuzo on 10/14/16.
//

#pragma once

#include "common/utility/DSeExprVariables.h"

#include <SeExpression.h>
#include <SeExprFunc.h>
#include <SeVec3d.h>
#include "SeExprNode.h"

#include "common/utility/DNumericValidation.h"
#include "common/utility/DSeExprCall.h"
#include "common/utility/DExpressionAttribute.h"
#include "DPointCommon.h"
#include "DPData.h"
#include "DPAttributeCommon.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>

class DPointSeExpr final : public SeExpression {
  public:
#pragma mark add DPGrpFunc
    struct addDPGrpFuncHelper : public SeExprFuncX {
      public:
        addDPGrpFuncHelper() : SeExprFuncX(true), m_pid(0U), m_dpoint(NULL), m_failed(NULL) {}
        bool prep(SeExprFuncNode* node, bool /*wantVec*/) {
            return dommy::expression::prepareNamedCall(node, 2, false);
        }
        void eval(const SeExprFuncNode* node, SeVec3d& result) const {
            std::string groupName;
            if (!dommy::expression::readCallName(node, 2, groupName)) {
                result = SeVec3d(0, 0, 0);
                fail();
                return;
            }

            SeVec3d r;
            node->child(1)->eval(r);
            if (!std::isfinite(r[0])) {
                result = SeVec3d(0, 0, 0);
                fail();
                return;
            }
            int groupStat = r[0] > 0 ? 1 : 0;

            result = SeVec3d(0, 0, 0);

            if (!m_dpoint) {
                fail();
                return;
            }
            if (m_pid >= m_dpoint->numberOfPoints()) {
                fail();
                return;
            }

            if (m_dpoint->isExistGrp(groupName)) {

                m_dpoint->getGrp(groupName)->editFromIndex(m_pid, groupStat);

            } else {
                DPIdList newGrpIds(m_dpoint->numberOfPoints(), false);
                if (m_pid < newGrpIds.size()) {
                    newGrpIds[m_pid] = groupStat;
                }
                m_dpoint->addGrp(groupName, DPGroupData(newGrpIds));
            }

            result = SeVec3d(1, 1, 1);
        }

        void setDPData(DPData& dpdata) {
            m_dpoint = &dpdata;
        }

        void setFailureFlag(bool* failed) {
            m_failed = failed;
        }

      public:
        std::size_t m_pid;

      private:
        void fail() const {
            if (m_failed)
                *m_failed = true;
        }

        DPData* m_dpoint;
        bool* m_failed;

    } addDPGrp;
    mutable SeExprFunc addDPGrpFunc;

#pragma mark get DPAttr
    class getDPAttrFuncHelper : public SeExprFuncX {
      public:
        getDPAttrFuncHelper() : SeExprFuncX(true), m_pid(0U), m_dpoint(NULL), m_failed(NULL) {}
        bool prep(SeExprFuncNode* node, bool /*wantVec*/) {
            return dommy::expression::prepareNamedCall(node, 1, false);
        }

        void eval(const SeExprFuncNode* node, SeVec3d& result) const {
            std::string attributeName;
            if (!dommy::expression::readCallName(node, 1, attributeName)) {
                result = SeVec3d(0, 0, 0);
                fail();
                return;
            }

            result = SeVec3d(0, 0, 0);

            if (!m_dpoint) {
                fail();
                return;
            }
            if (m_pid >= m_dpoint->numberOfPoints()) {
                fail();
                return;
            }

            if (m_dpoint->isExistAttr(attributeName)) {

                DP_ATTR_DATA_TYPE type = m_dpoint->getAttr(attributeName)->getAttrType();

                switch (type) {
                case DP_ATTR_BOOL: {
                    result[0] = result[1] = result[2] =
                        m_dpoint->attrListValueWithIndex<bool>(attributeName, m_pid);
                } break;
                case DP_ATTR_INT: {
                    result[0] = result[1] = result[2] =
                        m_dpoint->attrListValueWithIndex<int32_t>(attributeName, m_pid);
                    ;
                } break;
                case DP_ATTR_UINT32: {
                    result[0] = result[1] = result[2] =
                        m_dpoint->attrListValueWithIndex<uint32_t>(attributeName, m_pid);
                    ;
                } break;
                case DP_ATTR_INT64: {
                    double value = 0.0;
                    if (!dommy::numeric::checkedScalarCast(
                            m_dpoint->attrListValueWithIndex<int64_t>(attributeName, m_pid),
                            value, true)) {
                        fail();
                        return;
                    }
                    result[0] = result[1] = result[2] = value;
                } break;
                case DP_ATTR_FLOAT: {
                    result[0] = result[1] = result[2] =
                        m_dpoint->attrListValueWithIndex<float>(attributeName, m_pid);
                    ;
                } break;
                case DP_ATTR_DOUBLE: {
                    result[0] = result[1] = result[2] =
                        m_dpoint->attrListValueWithIndex<double>(attributeName, m_pid);
                    ;
                } break;
                case DP_ATTR_VEC2I: {
                    Imath::V2i val =
                        m_dpoint->attrListValueWithIndex<Imath::V2i>(attributeName, m_pid);
                    result[0] = val[0];
                    result[1] = val[1];
                } break;
                case DP_ATTR_VEC2F: {
                    Imath::V2f val =
                        m_dpoint->attrListValueWithIndex<Imath::V2f>(attributeName, m_pid);
                    result[0] = val[0];
                    result[1] = val[1];
                } break;
                case DP_ATTR_VEC2D: {
                    Imath::V2d val =
                        m_dpoint->attrListValueWithIndex<Imath::V2d>(attributeName, m_pid);
                    result[0] = val[0];
                    result[1] = val[1];
                } break;
                case DP_ATTR_VEC3I: {
                    Imath::V3i val =
                        m_dpoint->attrListValueWithIndex<Imath::V3i>(attributeName, m_pid);
                    result[0] = val[0];
                    result[1] = val[1];
                    result[2] = val[2];
                } break;
                case DP_ATTR_VEC3F: {
                    Imath::V3f val =
                        m_dpoint->attrListValueWithIndex<Imath::V3f>(attributeName, m_pid);
                    result[0] = val[0];
                    result[1] = val[1];
                    result[2] = val[2];
                } break;
                case DP_ATTR_VEC3D: {
                    Imath::V3d val =
                        m_dpoint->attrListValueWithIndex<Imath::V3d>(attributeName, m_pid);
                    result[0] = val[0];
                    result[1] = val[1];
                    result[2] = val[2];
                } break;
                default:
                    fail();
                    break;
                } // switch type

            } else {
                fail();
            } // check exist attribute
        }

        void setDPData(DPData& dpdata) {
            m_dpoint = &dpdata;
        }

        void setFailureFlag(bool* failed) {
            m_failed = failed;
        }

      public:
        std::size_t m_pid;

      private:
        void fail() const {
            if (m_failed)
                *m_failed = true;
        }

        DPData* m_dpoint;
        bool* m_failed;

    } getDPAttr;
    mutable SeExprFunc getDPAttrFunc;

#pragma mark set DPAttr
    class setDPAttrFuncHelper : public SeExprFuncX {
      public:
        setDPAttrFuncHelper() : SeExprFuncX(true), m_pid(0U), m_dpoint(NULL), m_failed(NULL) {}
        bool prep(SeExprFuncNode* node, bool wantVec) {
            return dommy::expression::prepareNamedCall(node, 2, wantVec);
        }

        void eval(const SeExprFuncNode* node, SeVec3d& result) const {
            std::string attributeName;
            if (!dommy::expression::readCallName(node, 2, attributeName)) {
                result = SeVec3d(0, 0, 0);
                fail();
                return;
            }

            SeVec3d r;
            bool isVec = false;

            if (node->child(1)->isVec()) {
                node->child(1)->eval(r);
                isVec = true;
            } else {
                SeVec3d tmp;
                node->child(1)->eval(tmp);
                r = SeVec3d(tmp[0], tmp[0], tmp[0]);
            }

            result = SeVec3d(0, 0, 0);

            if (!std::isfinite(r[0]) || !std::isfinite(r[1]) || !std::isfinite(r[2])) {
                fail();
                return;
            }

            if (!m_dpoint || m_pid >= m_dpoint->numberOfPoints()) {
                fail();
                return;
            }
            if (attributeName == "P") {
                float values[3] = {0.0f, 0.0f, 0.0f};
                if (!checkedFloatComponents(r, values)) {
                    fail();
                    return;
                }
                m_dpoint->setPosition(DPoint(values[0], values[1], values[2]), m_pid);
                result = SeVec3d(r[0], r[1], r[2]);
                return;
            }
            if (!m_dpoint->isExistAttr(attributeName)) {
                if (!isVec) {
                    float value = 0.0f;
                    if (!dommy::numeric::checkedFloatCast(r[0], value)) {
                        fail();
                        return;
                    }
                    if (!dommy::expression::createPointAttribute(
                            *m_dpoint, attributeName, m_pid, value)) {
                        fail();
                        return;
                    }
                } else {
                    float values[3] = {0.0f, 0.0f, 0.0f};
                    if (!checkedFloatComponents(r, values)) {
                        fail();
                        return;
                    }
                    if (!dommy::expression::createPointAttribute(
                            *m_dpoint, attributeName, m_pid,
                            Imath::V3f(values[0], values[1], values[2]))) {
                        fail();
                        return;
                    }
                }
            }

            if (m_dpoint->isExistAttr(attributeName)) {

                DP_ATTR_DATA_TYPE type = m_dpoint->getAttr(attributeName)->getAttrType();

                switch (type) {
                case DP_ATTR_BOOL: {
                    m_dpoint->setAttrListValueWithIndex<bool>(attributeName, m_pid, r[0] > 0.0);
                    result[0] = 1.0;
                } break;
                case DP_ATTR_INT: {
                    int32_t value = 0;
                    if (!checkedInteger(r[0], value)) {
                        fail();
                        return;
                    }
                    m_dpoint->setAttrListValueWithIndex<int32_t>(attributeName, m_pid, value);
                    result[0] = 1.0;
                } break;
                case DP_ATTR_UINT32: {
                    uint32_t value = 0U;
                    if (!checkedInteger(r[0], value)) {
                        fail();
                        return;
                    }
                    m_dpoint->setAttrListValueWithIndex<uint32_t>(attributeName, m_pid, value);
                    result[0] = 1.0;
                } break;
                case DP_ATTR_INT64: {
                    int64_t value = 0;
                    if (!checkedInteger(r[0], value)) {
                        fail();
                        return;
                    }
                    m_dpoint->setAttrListValueWithIndex<int64_t>(attributeName, m_pid, value);
                    result[0] = 1.0;
                } break;
                case DP_ATTR_FLOAT: {
                    float value = 0.0f;
                    if (!dommy::numeric::checkedFloatCast(r[0], value)) {
                        fail();
                        return;
                    }
                    m_dpoint->setAttrListValueWithIndex<float>(attributeName, m_pid, value);
                    result[0] = 1.0;
                } break;
                case DP_ATTR_DOUBLE: {
                    m_dpoint->setAttrListValueWithIndex<double>(attributeName, m_pid, r[0]);
                    result[0] = 1.0;
                } break;
                case DP_ATTR_VEC2I: {
                    int values[2] = {0, 0};
                    if (!checkedInteger(r[0], values[0]) || !checkedInteger(r[1], values[1])) {
                        fail();
                        return;
                    }
                    m_dpoint->setAttrListValueWithIndex<Imath::V2i>(
                        attributeName, m_pid, Imath::V2i(values[0], values[1]));
                    result[0] = result[1] = 1.0;
                } break;
                case DP_ATTR_VEC2F: {
                    float values[2] = {0.0f, 0.0f};
                    if (!dommy::numeric::checkedFloatCast(r[0], values[0]) ||
                        !dommy::numeric::checkedFloatCast(r[1], values[1])) {
                        fail();
                        return;
                    }
                    m_dpoint->setAttrListValueWithIndex<Imath::V2f>(
                        attributeName, m_pid, Imath::V2f(values[0], values[1]));
                    result[0] = result[1] = 1.0;
                } break;
                case DP_ATTR_VEC2D: {
                    m_dpoint->setAttrListValueWithIndex<Imath::V2d>(attributeName, m_pid,
                                                                    Imath::V2d(r[0], r[1]));
                    result[0] = result[1] = 1.0;
                } break;
                case DP_ATTR_VEC3I: {
                    int values[3] = {0, 0, 0};
                    if (!checkedInteger(r[0], values[0]) || !checkedInteger(r[1], values[1]) ||
                        !checkedInteger(r[2], values[2])) {
                        fail();
                        return;
                    }
                    m_dpoint->setAttrListValueWithIndex<Imath::V3i>(
                        attributeName, m_pid, Imath::V3i(values[0], values[1], values[2]));
                    result[0] = result[1] = result[2] = 1.0;
                } break;
                case DP_ATTR_VEC3F: {
                    float values[3] = {0.0f, 0.0f, 0.0f};
                    if (!checkedFloatComponents(r, values)) {
                        fail();
                        return;
                    }
                    m_dpoint->setAttrListValueWithIndex<Imath::V3f>(
                        attributeName, m_pid, Imath::V3f(values[0], values[1], values[2]));
                    result[0] = result[1] = result[2] = 1.0;
                } break;
                case DP_ATTR_VEC3D: {
                    m_dpoint->setAttrListValueWithIndex<Imath::V3d>(attributeName, m_pid,
                                                                    Imath::V3d(r[0], r[1], r[2]));
                    result[0] = result[1] = result[2] = 1.0;
                } break;
                default:
                    fail();
                    break;
                }

            } else {
                fail();
            }
        }

        void setDPData(DPData& dpdata) {
            m_dpoint = &dpdata;
        }

        void setFailureFlag(bool* failed) {
            m_failed = failed;
        }

      public:
        std::size_t m_pid;

      private:
        template <typename Integer> static bool checkedInteger(double value, Integer& result) {
            return dommy::numeric::checkedNumericCast(static_cast<long double>(value), result);
        }

        static bool checkedFloatComponents(const SeVec3d& value, float (&result)[3]) {
            return dommy::numeric::checkedFloatCast(value[0], result[0]) &&
                   dommy::numeric::checkedFloatCast(value[1], result[1]) &&
                   dommy::numeric::checkedFloatCast(value[2], result[2]);
        }

        void fail() const {
            if (m_failed)
                *m_failed = true;
        }

        DPData* m_dpoint;
        bool* m_failed;

    } setDPAttr;
    mutable SeExprFunc setDPAttrFunc;

    explicit DPointSeExpr(const std::string& expr)
        : SeExpression(expr), addDPGrpFunc(addDPGrp, 2, 2), getDPAttrFunc(getDPAttr, 1, 1),
          setDPAttrFunc(setDPAttr, 2, 2), m_evaluationFailed(false) {
        addDPGrp.setFailureFlag(&m_evaluationFailed);
        getDPAttr.setFailureFlag(&m_evaluationFailed);
        setDPAttr.setFailureFlag(&m_evaluationFailed);

    }

    void setDPData(DPData* dpdata) {
        if (!dpdata) {
            m_evaluationFailed = true;
            return;
        }
        if (usesFunc("addDPGroup"))
            addDPGrp.setDPData(*dpdata);
        if (usesFunc("getDPAttr"))
            getDPAttr.setDPData(*dpdata);
        if (usesFunc("setDPAttr"))
            setDPAttr.setDPData(*dpdata);
    }

    void setFrame(double p_input) {
        frame.val = p_input;
    }
    void setTime(double p_input) {
        time.val = p_input;
    }
    void setPos(const DPoint& p_input) {
        pos.val = SeVec3d(p_input[0], p_input[1], p_input[2]);
    }

    void setId(std::size_t p_input) {
        m_evaluationFailed = false;
        pid.val = addDPGrp.m_pid = getDPAttr.m_pid = setDPAttr.m_pid = p_input;
    }

    bool evaluationSucceeded() const {
        return !m_evaluationFailed;
    }

    bool usesPointDataFunctions() const {
        return usesFunc("getDPAttr") || usesFunc("setDPAttr") || usesFunc("addDPGroup");
    }

    void setUserFloat1(double p_input) {
        userFloat1.val = p_input;
    }
    void setUserFloat2(double p_input) {
        userFloat2.val = p_input;
    }
    void setUserFloat3(double p_input) {
        userFloat3.val = p_input;
    }
    void setUserFloat4(double p_input) {
        userFloat4.val = p_input;
    }

    void setUserVector1(const SeVec3d& p_input) {
        userVec1.val = p_input;
    }
    void setUserVector2(const SeVec3d& p_input) {
        userVec2.val = p_input;
    }
    void setUserVector3(const SeVec3d& p_input) {
        userVec3.val = p_input;
    }
    void setUserVector4(const SeVec3d& p_input) {
        userVec4.val = p_input;
    }

    void setUserColor1(const DPoint& p_input) {
        userColor1.val = SeVec3d(p_input[0], p_input[1], p_input[2]);
    }
    void setUserColor2(const DPoint& p_input) {
        userColor2.val = SeVec3d(p_input[0], p_input[1], p_input[2]);
    }
    void setUserColor3(const DPoint& p_input) {
        userColor3.val = SeVec3d(p_input[0], p_input[1], p_input[2]);
    }
    void setUserColor4(const DPoint& p_input) {
        userColor4.val = SeVec3d(p_input[0], p_input[1], p_input[2]);
    }

  private:
    bool m_evaluationFailed;

    using SimpleVar = dommy::expression::ScalarVariable;

    mutable SimpleVar frame;
    mutable SimpleVar time;
    mutable SimpleVar pid;

    using VecVar = dommy::expression::VectorVariable;

    // point attribute pos
    mutable VecVar pos;

    // user values
    mutable SimpleVar userFloat1;
    mutable SimpleVar userFloat2;
    mutable SimpleVar userFloat3;
    mutable SimpleVar userFloat4;

    mutable VecVar userVec1;
    mutable VecVar userVec2;
    mutable VecVar userVec3;
    mutable VecVar userVec4;

    mutable VecVar userColor1;
    mutable VecVar userColor2;
    mutable VecVar userColor3;
    mutable VecVar userColor4;

    SeExprVarRef* resolveVar(const std::string& name) const {
        if (name == "frame")
            return &frame;
        if (name == "time")
            return &time;
        if (name == "P")
            return &pos;
        if (name == "id")
            return &pid;
        if (name == "userFloat1")
            return &userFloat1;
        if (name == "userFloat2")
            return &userFloat2;
        if (name == "userFloat3")
            return &userFloat3;
        if (name == "userFloat4")
            return &userFloat4;
        if (name == "userVector1")
            return &userVec1;
        if (name == "userVector2")
            return &userVec2;
        if (name == "userVector3")
            return &userVec3;
        if (name == "userVector4")
            return &userVec4;
        if (name == "userColor1")
            return &userColor1;
        if (name == "userColor2")
            return &userColor2;
        if (name == "userColor3")
            return &userColor3;
        if (name == "userColor4")
            return &userColor4;
        return 0;
    }

    SeExprFunc* resolveFunc(const std::string& name) const {
        if (name == "addDPGroup")
            return &addDPGrpFunc;
        if (name == "getDPAttr")
            return &getDPAttrFunc;
        if (name == "setDPAttr")
            return &setDPAttrFunc;
        return 0;
    }
};
