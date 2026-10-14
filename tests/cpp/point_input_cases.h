#pragma once

// Classification tests execute the actual optional reader. Payload type and
// topology outcomes are explicit fixture seams; no DG ownership is emulated.
template <class Reset>
void optionalPointReaderCases(DPData& payload, bool& present,
                              const MObject& attribute, Reset reset)
{
    MDataBlock block;
    for (bool connected : {false, true}) {
        reset();
        pointInputFixture.connected = connected;
        const DPData* result = nullptr;
        assert(getOptionalInputDP({"owner"}, attribute, block, result) == MS::kSuccess);
        assert(result == &payload && callCount == 2 && pointInputFixture.dataReads == 1);

        reset();
        present = false;
        pointInputFixture.connected = connected;
        result = &payload;
        assert(getOptionalInputDP({"owner"}, attribute, block, result) ==
               (connected ? MS::kFailure : MS::kSuccess));
        assert(result == nullptr && callCount == 2 && operations.back() == "point.connection");
        for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
            reset();
            present = false;
            pointInputFixture.connected = connected;
            failAt = 1;
            injectedStatus = code;
            result = &payload;
            assert(getOptionalInputDP({"owner"}, attribute, block, result) == code);
            assert(result == nullptr && callCount == 2);
        }
    }
    for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
        reset();
        failAt = 0;
        injectedStatus = code;
        const DPData* result = &payload;
        assert(getOptionalInputDP({"owner"}, attribute, block, result) == code);
        assert(result == nullptr && callCount == 1 && pointInputFixture.dataReads == 0);
        reset();
        failAt = 1;
        injectedStatus = code;
        result = &payload;
        assert(getOptionalInputDP({"owner"}, attribute, block, result) == code);
        assert(result == nullptr && callCount == 2);
    }
    for (int invalid = 0; invalid < 4; ++invalid) {
        reset();
        if (invalid == 0) pointInputFixture.wrongMayaType = true;
        if (invalid == 1) pointInputFixture.wrongCppType = true;
        if (invalid == 2) pointInputFixture.nullRawData = true;
        if (invalid == 3) payload.topology = false;
        const DPData* result = &payload;
        assert(getOptionalInputDP({"owner"}, attribute, block, result) ==
               (invalid == 3 ? MS::kInvalidParameter : MS::kFailure));
        assert(result == nullptr);
    }
    for (int site : {0, 1}) {
        reset();
        attributeThrowAt = site;
        const DPData* result = &payload;
        try {
            (void)getOptionalInputDP({"owner"}, attribute, block, result);
            assert(false);
        } catch (const std::runtime_error&) {}
        assert(result == nullptr);
    }
    reset();
    present = false;
    attributeThrowAt = 1;
    const DPData* result = &payload;
    try {
        (void)getOptionalInputDP({"owner"}, attribute, block, result);
        assert(false);
    } catch (const std::runtime_error&) {}
    assert(result == nullptr);
    reset();
}
