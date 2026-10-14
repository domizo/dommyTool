//
//  DMetadataSelectCmd.cpp
//  dommyTool
//
//  Created by yuzo on 12/23/16.
//
//

#include "DMetadataSelectCmd.h"
#include "common/utility/DCommandGuard.h"
#include "common/string/DStreamFormat.h"




#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include <algorithm>
#include <sstream>
#include <string>

namespace {

class BoundedMetadataReport {
public:
    template <typename Value, typename... Rest>
    bool appendLine(const char* label, const Value& value, const Rest&... rest)
    {
        std::ostringstream line;
        line << label << value;
        // Initializer-list elements are sequenced left to right in C++11.
        const int inserted[] = {0, ((void)(line << rest), 0)...};
        (void)inserted;
        line << '\n';
        return append(dommy::text::checkedStreamText(line, "Metadata formatting failed"));
    }

    bool append(const std::string& line)
    {
        if (truncated_ || lineCount_ >= kMaxLineCount) {
            truncated_ = true;
            return false;
        }

        const std::size_t available =
            kMaxContentBytes > text_.size()
                ? kMaxContentBytes - text_.size()
                : 0U;
        const std::size_t copied = std::min(available, line.size());
        text_.append(line, 0U, copied);
        ++lineCount_;
        if (copied != line.size()) {
            truncated_ = true;
            return false;
        }
        if (lineCount_ >= kMaxLineCount ||
            text_.size() >= kMaxContentBytes) {
            truncated_ = true;
            return false;
        }
        return true;
    }

    bool full() const
    {
        return truncated_ || lineCount_ >= kMaxLineCount;
    }

    std::string finish() const
    {
        if (!truncated_) return text_;
        return text_ + "\n[metadata report truncated]\n";
    }

private:
    static const std::size_t kMaxContentBytes = 256U * 1024U;
    static const std::size_t kMaxLineCount = 10000U;
    std::string text_;
    std::size_t lineCount_ = 0U;
    bool truncated_ = false;
};

} // namespace

MStatus DMetadataSelectCmd::doIt( const MArgList& args )
{
	try {
	(void)args;
	MStatus status;
	MSelectionList activeList;
	status = MGlobal::getActiveSelectionList(activeList);
    if (status != MS::kSuccess) return status;

	MItSelectionList iter(activeList, MFn::kMesh, &status);
    if (status != MS::kSuccess) return status;
    if (iter.isDone()) {
        MGlobal::displayError("Select at least one polygon mesh.");
        return MS::kFailure;
    }

    BoundedMetadataReport report;
	for (; !iter.isDone() && !report.full();) {
        MDagPath meshPath;
        MObject selectedComponent;
        status = iter.getDagPath(meshPath, selectedComponent);
        if (status != MS::kSuccess || !meshPath.hasFn(MFn::kMesh)) {
            return status == MS::kSuccess ? MS::kFailure : status;
        }

        MFnMesh mesh(meshPath, &status);
        if (status != MS::kSuccess) return status;
        report.appendLine("mesh:", mesh.name().asChar());

        const adsk::Data::Associations* associations = mesh.metadata();
        if (associations == nullptr) {
            report.append("No metadata\n");
        } else {
            report.appendLine(
                "channel count:", associations->channelCount());
            for (unsigned int channelIndex = 0U;
                 channelIndex < associations->channelCount() &&
                 !report.full();
                 ++channelIndex) {
                const adsk::Data::Channel channel =
                    associations->channelAt(channelIndex);
                report.appendLine("channel name:", channel.name());
                report.appendLine(
                    "stream count:", channel.dataStreamCount());

                for (unsigned int streamIndex = 0U;
                     streamIndex < channel.dataStreamCount() &&
                     !report.full();
                     ++streamIndex) {
                    const adsk::Data::Stream* stream =
                        channel.dataStream(streamIndex);
                    if (stream == nullptr) continue;

                    report.appendLine("stream name:", stream->name());
                    report.appendLine(
                        "stream element count:", stream->elementCount());
                    report.appendLine(
                        "stream indexType:", stream->indexType());
                    report.appendLine(
                        "structure name:", stream->structure().name());
                    report.appendLine(
                        "structure totalSize:",
                        stream->structure().totalSize());
                    report.appendLine(
                        "structure member count:",
                        stream->structure().memberCount());

                    for (adsk::Data::Stream::const_iterator element =
                             stream->cbegin();
                         element != stream->cend() && !report.full();
                         ++element) {
                        adsk::Data::Handle handle(*element);
                        report.appendLine(
                            "index str:", element.index().asString());

                        for (adsk::Data::Structure::iterator member =
                                 stream->structure().begin();
                             member != stream->structure().end() &&
                             !report.full();
                             ++member) {
                            if (!handle.setPositionByMemberIndex(
                                    member.index())) {
                                MGlobal::displayError(
                                    "Unable to read a metadata member.");
                                return MS::kFailure;
                            }

                            for (unsigned int dimension = 0U;
                                 dimension < member->length() &&
                                 !report.full();
                                 ++dimension) {
                                std::string value = handle.str(dimension);
                                if (value.size() > 4096U) {
                                    value.resize(4096U);
                                    value += "...[value truncated]";
                                }
                                report.appendLine("structure name:", member->name(), ' ', value);
                            }
                        }
                    }
                }
            }
        }

        status = iter.next();
        if (status != MS::kSuccess) return status;
	}

    const std::string reportText = report.finish();
	if (!reportText.empty()) {
		MGlobal::displayInfo(reportText.c_str());
	}
	return MS::kSuccess;
	} DOMMY_COMMAND_CATCH("DMetadataSelectCmd")
}
