#ifndef CSLIBS_NDT_2D_SERIALIZATION_DYNAMIC_MAPS_GRIDMAP_HPP
#define CSLIBS_NDT_2D_SERIALIZATION_DYNAMIC_MAPS_GRIDMAP_HPP

#include <cslibs_ndt/common/serialization/indexed_distribution.hpp>
#include <cslibs_ndt/common/serialization/storage.hpp>

#include <cslibs_ndt_2d/dynamic_maps/gridmap.hpp>
#include <cslibs_math_2d/serialization/transform.hpp>

#include <yaml-cpp/yaml.h>

#include <cslibs_math/serialization/array.hpp>
#include <cslibs_ndt/common/serialization/filesystem.hpp>
#include <fstream>

namespace cslibs_ndt_2d {
namespace dynamic_maps {
inline void save(const cslibs_ndt_2d::dynamic_maps::Gridmap::distribution_storage_ptr_t &storage,
                 const boost::filesystem::path &directory)
{
    using distribution_t = cslibs_ndt_2d::dynamic_maps::Gridmap::distribution_t;
    using index_t = std::array<int, 2>;

    auto write = [&directory] (const index_t &i, const distribution_t &d) {
        boost::filesystem::path file =  directory /
                boost::filesystem::path("d" + std::to_string(i[0]) + "_" + std::to_string(i[1]));
        std::ofstream out(file.string());
        YAML::Node n;
        n["index"] = i;
        n["data"] = d.getHandle()->data();
        out << n;
    };
    storage->traverse(write);
}

inline bool save(const cslibs_ndt_2d::dynamic_maps::Gridmap::Ptr &map,
                 const std::string &path)
{
    using path_t  = boost::filesystem::path;
    using paths_t = std::array<path_t, 4>;
    using index_t = std::array<int, 2>;
    using distribution_storage_ptr_t   = cslibs_ndt_2d::dynamic_maps::Gridmap::distribution_storage_ptr_t;

    /// step one: check if the root diretory exists
    path_t path_root(path);
    if(!cslibs_ndt::common::serialization::check_directory(path_root)) {
        return false;
    }

    /// step two: check if the sub folders can be created
    paths_t paths = {{path_root / path_t("0"), path_root /  path_t("1"), path_root /  path_t("2"), path_root / path_t("3")}};

    /// step three: we have our filesystem, now we write out the distributions file by file
    /// meta file
    path_t path_file = path_t("map.yaml");
    {
        std::ofstream out = std::ofstream((path_root / path_file).string());
        YAML::Emitter yaml(out);
        YAML::Node n;
        std::vector<index_t> indices;
        map->getBundleIndices(indices);
        n["origin"] = map->getInitialOrigin();
        n["resolution"] = map->getResolution();
        n["min_index"] = map->getMinDistributionIndex();
        n["max_index"] = map->getMaxDistributionIndex();
        n["bundles"] = indices;
        yaml << n;

    }
    for(std::size_t i = 0 ; i < 4 ; ++i) {
        if(!cslibs_ndt::common::serialization::create_directory(paths[i])) {
            return false;
        }
    }

    const distribution_storage_ptr_t storage_0 = map->getStorages()[0];
    const distribution_storage_ptr_t storage_1 = map->getStorages()[1];
    const distribution_storage_ptr_t storage_2 = map->getStorages()[2];
    const distribution_storage_ptr_t storage_3 = map->getStorages()[3];

    save(storage_0, paths[0]);
    save(storage_1, paths[1]);
    save(storage_2, paths[2]);
    save(storage_3, paths[3]);

    return true;

}

inline bool load(const std::string &path)
{
    using path_t  = boost::filesystem::path;
    using paths_t = std::array<path_t, 4>;

    /// step one: check if the root diretory exists
    path_t path_root(path);
    if(!cslibs_ndt::common::serialization::check_directory(path_root)) {
        return false;
    }

    /// step two: check if the sub folders can be created
    paths_t paths = {{path_t("0"), path_t("1"), path_t("2"), path_t("3")}};

    /// step three: we have our filesystem, now we can load distributions file by file
    for(std::size_t i = 0 ; i < 4 ; ++i) {
        if(!cslibs_ndt::common::serialization::check_directory(paths[i])) {
            return false;
        }
    }

    return true;
}
}
}




namespace YAML {
template <>
struct convert<cslibs_ndt_2d::dynamic_maps::Gridmap::Ptr>
{
    using map_t = cslibs_ndt_2d::dynamic_maps::Gridmap;
    static Node encode(const typename map_t::Ptr &rhs)
    {
        Node n;
        if (!rhs)
            return n;

        n.push_back(rhs->getInitialOrigin());
        n.push_back(rhs->getResolution());

        using index_t = std::array<int, 2>;
        const index_t min_distribution_index = rhs->getMinDistributionIndex();
        const index_t max_distribution_index = rhs->getMaxDistributionIndex();
        n.push_back(min_distribution_index);
        n.push_back(max_distribution_index);

        using distribution_storage_t = typename map_t::distribution_storage_t;
        using distribution_storage_ptr_t = typename map_t::distribution_storage_ptr_t;

        auto divx = [](const index_t & bi) { return cslibs_math::common::div<int>(bi[0], 2); };
        auto divy = [](const index_t & bi) { return cslibs_math::common::div<int>(bi[1], 2); };
        auto modx = [](const index_t & bi) { return cslibs_math::common::mod<int>(bi[0], 2); };
        auto mody = [](const index_t & bi) { return cslibs_math::common::mod<int>(bi[1], 2); };

        auto get_storage_index = [&divx, &divy, &modx, &mody](const index_t & bi, const std::size_t i) {
            return index_t({{(i % 2 == 0) ? divx(bi) : (divx(bi) + modx(bi)),
                             (i < 2) ? divy(bi) : (divy(bi) + mody(bi))}});
        };

        for (std::size_t i = 0 ; i < 4 ; ++ i) {
            const distribution_storage_ptr_t storage(new distribution_storage_t());

            for (int idx = min_distribution_index[0] ; idx <= max_distribution_index[0] ; ++ idx) {
                for (int idy = min_distribution_index[1] ; idy <= max_distribution_index[1] ; ++ idy) {
                    const index_t bi({idx, idy});

                    if (const typename map_t::distribution_bundle_t* b = rhs->getDistributionBundle(bi)) {
                        const index_t si = get_storage_index(bi, i);
                        if (!storage->get(si) && b->at(i)->data().getN() > 0)
                            storage->insert(si, *(b->at(i)));
                    }
                }
            }

            n.push_back(storage);
        }

        return n;
    }

    static bool decode(const Node& n, typename map_t::Ptr &rhs)
    {
        if (!n.IsSequence() || n.size() != 8)
            return false;

        rhs.reset(new map_t(n[0].as<cslibs_math_2d::Transform2d>(), n[1].as<double>()));

        using index_t = std::array<int, 2>;
        const index_t min_distribution_index = n[2].as<index_t>();
        const index_t max_distribution_index = n[3].as<index_t>();

        using distribution_storage_ptr_t = typename map_t::distribution_storage_ptr_t;

        auto get_bundle_index = [&min_distribution_index, &max_distribution_index] (const index_t & si) {
            return index_t({{std::max(min_distribution_index[0], std::min(2 * si[0], max_distribution_index[0])),
                             std::max(min_distribution_index[1], std::min(2 * si[1], max_distribution_index[1]))}});
        };

        for (std::size_t i = 0 ; i < 4 ; ++ i) {
            const distribution_storage_ptr_t & storage = n[4 + i].as<distribution_storage_ptr_t>();

            storage->traverse([&rhs, &i, &get_bundle_index] (const index_t & si, const typename map_t::distribution_t & d) {
                const index_t & bi = get_bundle_index(si);
                if (const typename map_t::distribution_bundle_t* b = rhs->getDistributionBundle(bi))
                    b->at(i)->data() = d.data();
            });
        }

        return true;
    }
};
}

#endif // CSLIBS_NDT_2D_SERIALIZATION_DYNAMIC_MAPS_GRIDMAP_HPP
