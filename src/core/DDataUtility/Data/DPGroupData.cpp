//
//  DPGroupData.cpp
//  dommyTool
//
//  Created by yuzo on 5/25/16.
//
//

#include "DPGroupData.h"
#include "common/utility/DIndexTransform.h"

DPGroupData::DPGroupData(const DPIdList& values) {
    m_IdArray = values;
}

DPGroupData::Ptr DPGroupData::copy() const {
    DPGroupData::Ptr attr(new DPGroupData());
    attr->copy(*this);
    return attr;
}

void DPGroupData::copy(const DPGroupData& other) {
    m_IdArray = other.m_IdArray;
}

/// グループ内のアクティブなインデックスを返す
void DPGroupData::getActiveIndexList(
        DPIndexList& indexList) const {

    size_t index = 0;
    for (auto it = m_IdArray.begin(); it != m_IdArray.end(); ++it) {
        if (*it) {
            indexList.push_back(index);
        }

        index++;
    }
}

void DPGroupData::setActiveFromIndex(const size_t& idx) {
    if (idx < m_IdArray.size()) {
        m_IdArray[idx] = true;
    }
}

void DPGroupData::setDeActiveFromIndex(const size_t& idx) {
    if (idx < m_IdArray.size()) {
        m_IdArray[idx] = false;
    }
}

void DPGroupData::editFromIndex(const size_t& idx, const bool& state) {
    if (idx < m_IdArray.size()) {
        m_IdArray[idx] = state;
    }
}

void DPGroupData::deleteArrayFromIds(const DPIdList& ids) {
    DPIdList tmpIds(m_IdArray);

    if (ids.size() == tmpIds.size()) {
        m_IdArray.clear();

        for (uint i = 0; i < tmpIds.size(); i++) {
            if (!ids[i]) {
                m_IdArray.push_back(tmpIds[i]);
            }
        }
    }
}

void DPGroupData::sortArrayFromIds(const DPIndexList& ids) {
    (void)dommy::container::reorderInPlace(m_IdArray, ids);
}

void DPGroupData::remapFromIds(const DPIndexList& ids) {
    (void)dommy::container::remapInPlace(m_IdArray, ids);
}

void DPGroupData::appendOtherGrp(const DPGroupData::ConstPtr& other) {
    if (!other) return;
    const DPIdList& values = other->getGroupIds();
    m_IdArray.insert(m_IdArray.end(), values.begin(), values.end());
}

void DPGroupData::expandOtherGrp(const size_t& count) {
    (void)dommy::container::insertFill(
        m_IdArray, m_IdArray.size(), count, false);
}

void DPGroupData::expandOtherGrpFromIndex(const size_t& index) {
    if (index < m_IdArray.size()) {

        bool val = m_IdArray[index];
        m_IdArray.push_back(val);
    } else {
        m_IdArray.push_back(false);
    }
}

void DPGroupData::insertOtherGrp(const size_t& count, const unsigned int& index,
                                 const bool& state) {
    (void)dommy::container::insertFill(
        m_IdArray, static_cast<std::size_t>(index), count, state);
}

bool DPGroupData::isExistActive() const {

    if (std::find(m_IdArray.begin(), m_IdArray.end(), 1) == m_IdArray.end()) {
        // not containd yet
        return false;
    } else {
        return true;
    }
}
