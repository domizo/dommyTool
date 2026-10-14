//
//  DStringOp.cpp
//  dommyTool
//
//  Created by yuzo on 7/10/16.
//
//

#include "DStringOp.h"
#include "common/string/DStreamFormat.h"

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <sstream>

//whitespaceの削除
std::string eraseWhitespace(const std::string& str) {
	if (!str.empty()) {
		std::string tmpStr(str);
		tmpStr.erase(std::remove(tmpStr.begin(),tmpStr.end(),' '),tmpStr.end());
		
		return tmpStr;
	} else {
		return std::string("");
	}
}


std::vector<std::string> splitStringWithChar(
    const std::string& baseString,
    const std::string& delimiters) {
	std::vector<std::string> selectionList;
    std::string current;
    for (std::string::const_iterator it = baseString.begin();
         it != baseString.end();
         ++it) {
        if (delimiters.find(*it) != std::string::npos) {
            selectionList.push_back(current);
            current.clear();
        } else {
            current.push_back(*it);
        }
    }
    selectionList.push_back(current);
	
	return selectionList;
}

void IntersectDStringList(DStringList &baseList, const DStringList &newlist) {
    for (auto n:newlist) {
        if(std::find(baseList.begin(), baseList.end(), n) == baseList.end()) {
            baseList.push_back(n);
        }
    }
}

std::string mergeStringList(const std::vector<std::string>& strlist)
{
    std::ostringstream stream;
    for (std::size_t index = 0U; index < strlist.size(); ++index) {
        if (index != 0U) stream << ' ';
        stream << strlist[index];
    }
    return dommy::text::checkedStreamText(stream, "String list formatting failed");
}


std::string formattedFileSize(unsigned long long size) {
    static const unsigned long long kKibibyte = 1024ULL;
    static const unsigned long long kMebibyte = kKibibyte*kKibibyte;
    static const unsigned long long kGibibyte = kMebibyte*kKibibyte;

    std::ostringstream stream;
    if (size < kKibibyte) {
        stream << size << " bytes";
    } else if (size < kMebibyte) {
        stream << static_cast<double>(size)/kKibibyte << " KB";
    } else if (size < kGibibyte) {
        stream << static_cast<double>(size)/kMebibyte << " MB";
    } else {
        stream << static_cast<double>(size)/kGibibyte << " GB";
    }
    return dommy::text::checkedStreamText(stream, "File size formatting failed");
}

std::string formattedNumberWithComma(const size_t number) {
    const std::string digits = std::to_string(number);
    std::string result;
    result.reserve(digits.size() + (digits.size() - 1U) / 3U);

    for (size_t i = 0; i < digits.size(); ++i) {
        if (i != 0U && (digits.size() - i) % 3U == 0U) result += ',';
        result += digits[i];
    }
    return result;
}



void removeLastExtention(std::string &base) {
	size_t lastindex = base.find_last_of(".");
	base = base.substr(0, lastindex);
}



void checkValidString(std::string &srcString, const std::string& defaultString) {
	
	if(srcString.empty()){
		srcString = defaultString;
	}
	
}


bool isContainString(const std::string& str, const std::string& targetStr) {
	if (str.find(targetStr) != std::string::npos) {
		return true;
	}
	return false;
}

bool isMatchPrefixString(const std::string& str, const std::string& targetStr) {
	size_t pos = str.find(targetStr);
	if(pos == 0) {
		return true;
	}
	return false;
}


bool isContainDStringList(const DStringList &strlist, const std::string& str) {
    
    if(std::find(strlist.begin(), strlist.end(), str) == strlist.end()) {
        return false;
    } else {
        return true;
    }
    
}
