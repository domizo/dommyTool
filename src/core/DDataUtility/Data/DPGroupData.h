//
//  DPGroupData.hpp
//  dommyTool
//
//  Created by yuzo on 5/25/16.
//
//

#ifndef DPGroupData_h
#define DPGroupData_h

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "DType.h"

class DPGroupData {
  public:
    typedef std::shared_ptr<DPGroupData> Ptr;
    typedef std::shared_ptr<const DPGroupData> ConstPtr;

    DPGroupData() {};
    DPGroupData(const DPIdList& values);
    virtual ~DPGroupData() {}

    // copy method
    virtual DPGroupData::Ptr copy() const;
    virtual void copy(const DPGroupData& other);

    inline size_t numberOfIds() const {
        return std::count(m_IdArray.begin(), m_IdArray.end(), 1);
    };

    inline const DPIdList& getGroupIds() const {
        return m_IdArray;
    };

    void getActiveIndexList(DPIndexList& indexList) const;

    //	inline void updateIdCount();
    void setActiveFromIndex(const size_t& idx);
    void setDeActiveFromIndex(const size_t& idx);
    void editFromIndex(const size_t& idx, const bool& state);

    void deleteArrayFromIds(const DPIdList& ids);
    void sortArrayFromIds(const DPIndexList& ids);
    void remapFromIds(const DPIndexList& ids);

    void appendOtherGrp(const DPGroupData::ConstPtr& other);
    void expandOtherGrp(const size_t& count);
    void expandOtherGrpFromIndex(const size_t& index);
    void insertOtherGrp(const size_t& count, const unsigned int& index, const bool& state);

    bool isExistActive() const;

  private:
    DPIdList m_IdArray;
};

typedef std::unordered_map<std::string, DPGroupData::Ptr> DPGrpList;
typedef DPGrpList::iterator DPGrpIter;
typedef DPGrpList::const_iterator ConstDPGrpIter;

#endif /* DPGroupData_h */
