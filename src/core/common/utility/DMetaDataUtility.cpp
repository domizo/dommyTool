//
//  DMetaDataUtility.cpp
//  dommyTool
//
//  Created by yuzo on 12/24/16.
//
//

#include "DMetaDataUtility.h"





adsk::Data::Structure* findMetadataStructure(
    const std::string& structureName)
{
    adsk::Data::Structure::List& structures =
        adsk::Data::Structure::allStructures();
    for (adsk::Data::Structure::ListIterator iterator = structures.begin();
         iterator != structures.end(); ++iterator) {
        if (*iterator &&
            std::string((*iterator)->name()) == structureName) {
            return *iterator;
        }
    }
    return NULL;
}
