//
//  DStringOp.hpp
//  dommyTool
//
//  Created by yuzo on 7/10/16.
//
//

#ifndef DStringOp_h
#define DStringOp_h

#include <cstddef>
#include <string>
#include <vector>

#include "DType.h"


std::string eraseWhitespace(const std::string& str);
std::vector<std::string> splitStringWithChar(
    const std::string& baseString,
    const std::string& delimiters);

std::string formattedFileSize(unsigned long long size);
std::string formattedNumberWithComma(const size_t number);

std::string mergeStringList(const std::vector<std::string>& strlist);

void IntersectDStringList(DStringList &baseList, const DStringList &newlist);




void removeLastExtention(std::string &base);

void checkValidString(std::string &srcString, const std::string& defaultString);

bool isContainString(const std::string& str, const std::string& targetStr);
bool isMatchPrefixString(const std::string& str, const std::string& targetStr);

bool isContainDStringList(const DStringList &strlist, const std::string& str);






#endif /* DStringOp_h */
