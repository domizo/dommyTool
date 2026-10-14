//
//  DMayaDebug.h
//  dommyTool
//
//  Created by yuzo on 2015/03/18.
//
//

#ifndef dommyTool_DMayaDebug_h
#define dommyTool_DMayaDebug_h

#include "DLogConfig.h"

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sstream>
#include <string>
#include <chrono>
#include <vector>


#include <maya/MFnDependencyNode.h>
#include <maya/MString.h>
#include <maya/MObject.h>
#include <maya/MVector.h>
#include <maya/MGlobal.h>

#define MERR_CHK(status,msg) if ( !status ) { MGlobal::displayError(msg); } // cerr << msg << endl; }

#define McheckErr(stat, msg)		\
if ( MS::kSuccess != stat )		\
{								\
MGlobal::displayError(msg);	\
return MS::kFailure;		\
}


inline void EchoInfo(const std::string& nodeName, const std::string& message) {
    std::stringstream msg;
    msg << nodeName.c_str() << " : " << message.c_str();
    MGlobal::displayInfo(msg.str().c_str());
}

inline void EchoWarning(const std::string& nodeName, const std::string& errorMessage) {
	std::stringstream msg;
	msg << nodeName.c_str() << " : " << errorMessage.c_str();
	MGlobal::displayWarning(msg.str().c_str());
}

inline void EchoError(const std::string& nodeName, const std::string& errorMessage) {
    std::stringstream msg;
    msg << nodeName.c_str() << " : " << errorMessage.c_str();
    MGlobal::displayError(msg.str().c_str());
}

inline void EchoDebug(const std::string& nodeName, const std::string& message) {
#if DOMMY_ENABLE_DEBUG_LOGS
    EchoInfo(nodeName, message);
#else
    (void)nodeName;
    (void)message;
#endif
}

#define DOMMY_MAYA_ERROR(nodeName, streamExpression)                         \
    do {                                                                     \
        std::ostringstream dommyMayaLogStream;                               \
        dommyMayaLogStream << streamExpression;                              \
        EchoError(nodeName, dommyMayaLogStream.str());                       \
    } while (false)

#define DOMMY_MAYA_WARNING(nodeName, streamExpression)                       \
    do {                                                                     \
        std::ostringstream dommyMayaLogStream;                               \
        dommyMayaLogStream << streamExpression;                              \
        EchoWarning(nodeName, dommyMayaLogStream.str());                     \
    } while (false)

#define DOMMY_MAYA_DEBUG(nodeName, streamExpression)                         \
    do {                                                                     \
        if (DOMMY_ENABLE_DEBUG_LOGS) {                                        \
            std::ostringstream dommyMayaLogStream;                           \
            dommyMayaLogStream << streamExpression;                          \
            EchoDebug(nodeName, dommyMayaLogStream.str());                   \
        }                                                                    \
    } while (false)


inline MString getDMNodeInfo(MObject object) {
	MFnDependencyNode fn(object);
	MString nodeName=fn.name();
	MString typeName=fn.typeName();
	
	
	return MString(typeName+":"+nodeName+":");
};


inline void outputCalcTime(std::chrono::time_point<std::chrono::system_clock> start,
						   std::chrono::time_point<std::chrono::system_clock> end) {
	auto diff = end - start;
	std::stringstream message;
	message << "elapsed time = "
	        << std::chrono::duration_cast<std::chrono::milliseconds>(diff).count()
	        << " msec.";
	EchoDebug("Timing", message.str());
}

template <typename T>
void EchoScalarVectorList(const std::vector<T> &datalist)
{
    std::stringstream message;
    message << "Data list size(" << datalist.size() << "):";
    for (size_t i=0,N=datalist.size(); i<N; ++i) {
        message << " " << datalist[i];
    }
    EchoDebug("Data", message.str());
}



#pragma mark time method
/*
 usage
 double startTime = timeInSeconds();
 
 //anything
 
 float elapTime = (float) (timeInSeconds() - startTime);
 printf(" %fs \n",elapTime);
*/
inline  double timeInSeconds() {
	struct timeval tv;
	struct timezone tz;
	
	gettimeofday(&tv, &tz);
	return tv.tv_sec + tv.tv_usec/1000000.0;
}



#endif
