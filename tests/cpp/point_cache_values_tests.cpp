#include "point_cache_value_doubles.h"

#include "point_cache_values.inc"

template <typename Fn> void rejects(Fn fn) {
    bool rejected = false;
    try {
        fn();
    } catch (const std::exception&) {
        rejected = true;
    }
    assert(rejected);
}
template <typename T>
void field(DPData& data, const std::string& name, const std::vector<T>& values, const T& initial) {
    data.attributes[name] = std::make_shared<TypedDPAttributeData<T>>(values, initial);
}
openvdb::points::PointDataGrid::Ptr grid() {
    auto result = std::make_shared<openvdb::points::PointDataGrid>();
    auto ids = std::make_shared<VDBArray<openvdb::Index>>();
    ids->values = {1U, 0U};
    result->tree().leaf.attributes.arrays["id"] = ids;
    return result;
}
bool equal(const MMatrix& a, const MMatrix& b) {
    for (unsigned int r = 0; r < 4; ++r)
        for (unsigned int c = 0; c < 4; ++c)
            if (a[r][c] != b[r][c])
                return false;
    return true;
}
void matrices() {
    MMatrix first, second, initial;
    for (unsigned int r = 0; r < 4; ++r)
        for (unsigned int c = 0; c < 4; ++c) {
            first[r][c] = r * 4 + c + 0.25;
            second[r][c] = -(r * 4 + c + 1.5);
            initial[r][c] = r * 4 + c + 3.75;
        }
    // Compare the encoded components independently of round-trip symmetry.
    openvdb::Mat4d encoded;
    assert(copyCacheMatrix(first, encoded));
    for (unsigned int r = 0; r < 4; ++r)
        for (unsigned int c = 0; c < 4; ++c)
            assert(encoded[r][c] == first[r][c]);
    MMatrix restored;
    assert(copyCacheMatrix(encoded, restored) && equal(first, restored));
    for (unsigned int r = 0; r < 4; ++r)
        for (unsigned int c = 0; c < 4; ++c) {
            for (double bad : {NAN, INFINITY, -INFINITY}) {
                MMatrix invalid = first, destination = second;
                invalid[r][c] = bad;
                assert(!copyCacheMatrix(invalid, destination) && equal(destination, second));
            }
        }
    DPData source, output;
    field<MMatrix>(source, "matrix", {first, second}, initial);
    preflightCacheInput(source);
    auto data = grid();
    auto indices = std::make_shared<openvdb::tools::PointIndexGrid>();
    writeDPAttributeToOpnVDBPointAttribute<MMatrix>(source, data, indices, "matrix");
    CacheWorkspace workspace;
    preflightCacheRead(2, *data, MStringArray{{MString("matrix")}}, workspace);
    writeOpenVDBPointAttributeToDPAttribute<MMatrix>(output, 2, "matrix", data,
                                                     data->tree().leaf.attributes, workspace);
    auto result =
        std::dynamic_pointer_cast<const TypedDPAttributeData<MMatrix>>(output.getAttr("matrix"));
    assert(result && equal(result->value()[0], first) && equal(result->value()[1], second));
    assert(equal(result->getDefaultValue(), initial));
    field<double>(source, "matrix", {1, 2}, 0);
    rejects(
        [&] { writeDPAttributeToOpnVDBPointAttribute<MMatrix>(source, data, indices, "matrix"); });
    auto matrixArray =
        std::dynamic_pointer_cast<DVDBAttrMat>(data->tree().leaf.attributes.arrays["matrix"]);
    matrixArray->values[0][3][2] = NAN;
    rejects([&] {
        writeOpenVDBPointAttributeToDPAttribute<MMatrix>(output, 2, "matrix", data,
                                                         data->tree().leaf.attributes, workspace);
    });
    assert(output.getAttr("matrix") == result && equal(result->value()[0], first));
    matrixArray->values[0][3][2] = second[3][2];
    auto matrixDefault = std::dynamic_pointer_cast<openvdb::TypedMetadata<openvdb::Mat4d>>(
        data->tree().leaf.attributes.defaults["matrix"]);
    const double savedDefault = matrixDefault->value[2][3];
    matrixDefault->value[2][3] = INFINITY;
    rejects([&] {
        writeOpenVDBPointAttributeToDPAttribute<MMatrix>(output, 2, "matrix", data,
                                                         data->tree().leaf.attributes, workspace);
    });
    assert(output.getAttr("matrix") == result && equal(result->getDefaultValue(), initial));
    matrixDefault->value[2][3] = savedDefault;
    auto ids = std::dynamic_pointer_cast<VDBArray<openvdb::Index>>(
        data->tree().leaf.attributes.arrays["id"]);
    ids->values[0] = 2U;
    rejects([&] {
        writeOpenVDBPointAttributeToDPAttribute<MMatrix>(output, 2, "matrix", data,
                                                         data->tree().leaf.attributes, workspace);
    });
    assert(output.getAttr("matrix") == result);
    ids->values[0] = 1U;
    writeOpenVDBPointAttributeToDPAttribute<MMatrix>(output, 2, "matrix", data,
                                                     data->tree().leaf.attributes, workspace);
    auto recovered =
        std::dynamic_pointer_cast<const TypedDPAttributeData<MMatrix>>(output.getAttr("matrix"));
    assert(recovered && equal(recovered->value()[0], first) &&
           equal(recovered->value()[1], second) && equal(recovered->getDefaultValue(), initial));
}
template <typename T> bool same(const T& a, const T& b) {
    return a == b;
}
template <typename T> bool same(const Imath::Vec2<T>& a, const Imath::Vec2<T>& b) {
    return a[0] == b[0] && a[1] == b[1];
}
template <typename T> bool same(const Imath::Vec3<T>& a, const Imath::Vec3<T>& b) {
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}
template <typename T> void roundTrip(const T& a, const T& b, const T& initial) {
    DPData source, output;
    field<T>(source, "field", {a, b}, initial);
    preflightCacheInput(source);
    auto data = grid();
    auto indices = std::make_shared<openvdb::tools::PointIndexGrid>();
    writeDPAttributeToOpnVDBPointAttribute<T>(source, data, indices, "field");
    CacheWorkspace workspace;
    preflightCacheRead(2, *data, MStringArray{{MString("field"), MString("missing")}}, workspace);
    writeOpenVDBPointAttributeToDPAttribute<T>(output, 2, "field", data,
                                               data->tree().leaf.attributes, workspace);
    auto result = std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(output.getAttr("field"));
    assert(result && result->value().size() == 2);
    assert(same(T(result->value()[0]), a) && same(T(result->value()[1]), b));
    assert(same(result->getDefaultValue(), initial));
}
void budget() {
    const std::size_t maximum = 64U * 1024U * 1024U; // Shared four-copy allowance.
    CacheWorkspace exact;
    exact.add(maximum, 1U);
    rejects([&] { exact.add(1U, 1U); });
    exact.add(0U, 1U);
    CacheWorkspace overflow;
    rejects([&] { overflow.add(std::numeric_limits<std::size_t>::max(), 2U); });
    CacheWorkspace many;
    many.add(maximum / 2U, 1U);
    rejects([&] { many.add(maximum / 2U + 1U, 1U); });
    DPData large;
    large.count = 10000000U;
    rejects([&] { preflightCacheInput(large); });
    DPData source;
    field<double>(source, "bad", {1, INFINITY}, 0);
    rejects([&] { preflightCacheInput(source); });
    field<double>(source, "bad", {1}, 0);
    rejects([&] { preflightCacheInput(source); });
    source.attributes.clear();
    source.groups["bad"] = std::make_shared<Group>();
    rejects([&] { preflightCacheInput(source); });
    source.groups.clear();
    source.valid = false;
    rejects([&] { preflightCacheInput(source); });
    source.valid = true;
    source.count = 0;
    preflightCacheInput(source);
    auto data = grid();
    CacheWorkspace read;
    rejects([&] { preflightCacheRead(10000000U, *data, MStringArray{}, read); });
    for (const char* name : {"m1", "m2", "m3"})
        data->tree().leaf.attributes.arrays[name] = std::make_shared<DVDBAttrMat>();
    CacheWorkspace two;
    preflightCacheRead(200000U, *data, MStringArray{{MString("m1"), MString("m2")}}, two);
    CacheWorkspace three;
    rejects([&] {
        preflightCacheRead(200000U, *data,
                           MStringArray{{MString("m1"), MString("m2"), MString("m3")}}, three);
    });
    CacheWorkspace stringBudget;
    stringBudget.add(maximum - 2U, 1U);
    rejects([&] { validateCacheValue(std::string("three"), stringBudget); });
    rejects([&] { validateCacheValue(std::string("a\0b", 3), stringBudget); });
    CacheWorkspace scalarBudget;
    rejects([&] { validateCacheValue(INFINITY, scalarBudget); });
}
int main() {
    matrices();
    budget();
    roundTrip<bool>(true, false, true);
    roundTrip<int32_t>(-3, 8, 1);
    roundTrip<uint32_t>(3, 8, 1);
    roundTrip<int64_t>(-3000000000LL, 8000000000LL, 1);
    roundTrip<float>(1.25f, -3.5f, 0.5f);
    roundTrip<double>(1.25, -3.5, 0.5);
    roundTrip<Imath::V2i>({1, 2}, {3, 4}, {5, 6});
    roundTrip<Imath::V2f>({1, 2}, {3, 4}, {5, 6});
    roundTrip<Imath::V2d>({1, 2}, {3, 4}, {5, 6});
    roundTrip<Imath::V3i>({1, 2, 3}, {4, 5, 6}, {7, 8, 9});
    roundTrip<Imath::V3f>({1, 2, 3}, {4, 5, 6}, {7, 8, 9});
    roundTrip<Imath::V3d>({1, 2, 3}, {4, 5, 6}, {7, 8, 9});
    roundTrip<std::string>("first", "second", "default");
    std::cout << "DPCache: 14 typed codec round trips, all 16 matrix components/defaults, 48 "
                 "nonfinite matrix cases and aggregate budgets passed\n";
}
