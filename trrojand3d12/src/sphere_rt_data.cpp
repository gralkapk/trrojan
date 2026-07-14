#include "trrojan/d3d12/sphere_rt_data.h"

#include <mmpld.h>

#include <glm/glm.hpp>

#include "trrojan/com_error_category.h"
#include "trrojan/d3d12/utilities.h"

namespace trrojan::d3d12 {
sphere_rt_data::sphere_rt_data() : _bbox({glm::vec3(0.0f), glm::vec3(0.0f)}), _max_radius(0.0f) {}

winrt::com_ptr<ID3D12Resource> sphere_rt_data::load(
    ID3D12GraphicsCommandList* command_list, std::string const& path, std::uint32_t frame) {
    assert(command_list != nullptr);
    auto device = get_device(command_list);
    winrt::com_ptr<ID3D12Resource> retval;

    try {
        // TODO parse random sphere description
        random_sphere_generator::description desc = random_sphere_generator::parse_description(
            path, random_sphere_generator::create_flags::per_particle_radius |
                      random_sphere_generator::create_flags::per_particle_radius);

        log::instance().write_line(log_level::information,
            "Create random "
            "sphere data set \"{}\".",
            path);
        auto const size = random_sphere_generator::create(nullptr, 0, desc);

        data_ = create_buffer(device, size);
        assert(data_ != nullptr);
        retval = create_upload_buffer_for(data_);

        assert(retval != nullptr);
        void* data;
        auto hr = retval->Map(0, nullptr, &data);
        if (FAILED(hr)) {
            throw std::system_error(hr, com_category());
        }

        try {
            random_sphere_generator::create(data, size, this->_max_radius, desc);
        } catch (...) {
            log::instance().write_line(log_level::error,
                "Failed to create "
                "random sphere data for specification \"{}\". The "
                "specification was correct, but the generation threw an "
                "exception.",
                path);
            throw;
        }

        _cnt_spheres = desc.number;
        for (glm::length_t i = 0; i < this->_bbox[0].length(); ++i) {
            this->_bbox[0][i] = -0.5f * desc.domain_size[i];
            this->_bbox[1][i] = 0.5f * desc.domain_size[i];
        }

        retval->Unmap(0, nullptr);

        set_debug_object_name(data_.get(), path.c_str());

        command_list->CopyBufferRegion(data_.get(), 0, retval.get(), 0, size);
        transition_resource(
            command_list, data_.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
    } catch (...) {
        // Interpret it as the path to an MMPLD file.
        mmpld::list_header list_header;
        mmpld::file<HANDLE> file(path.c_str());
        file.open_frame(frame);
        file.read_particles(false, list_header, nullptr, 0);

        // patch color to rgba8
        auto const initial_color = list_header.colour_type;
        list_header.colour_type = mmpld::colour_type::rgba8;

        // path vertex to float4
        auto const initial_vertex = list_header.vertex_type;
        list_header.vertex_type = mmpld::vertex_type::float_xyzr;

        auto const size = mmpld::get_size<UINT64>(list_header);
        data_ = create_buffer(device, size);
        assert(data_ != nullptr);
        retval = create_upload_buffer_for(data_);

        assert(retval != nullptr);
        void* data;
        auto hr = retval->Map(0, nullptr, &data);
        if (FAILED(hr)) {
            throw std::system_error(hr, com_category());
        }

        try {
            if (initial_color != list_header.colour_type || initial_vertex != list_header.vertex_type) {
                auto src_header = list_header;
                src_header.colour_type = initial_color;
                src_header.vertex_type = initial_vertex;
                std::vector<std::uint8_t> buffer(mmpld::get_size<std::size_t>(src_header));
                file.read_particles(src_header, buffer.data(), size);
                mmpld::convert(buffer.data(), src_header, data, list_header);
            } else {
                file.read_particles(false, list_header, data, size);
            }

            fit_bounding_box(list_header, data);
        } catch (...) {
            log::instance().write_line(log_level::error,
                "Failed to read "
                "MMPLD particles from \"{}\". The headers could be read, i.e."
                "the input file might be corrupted.",
                path);
            throw;
        }

        _cnt_spheres = list_header.particles;

        retval->Unmap(0, nullptr);

        set_debug_object_name(data_.get(), path.c_str());

        command_list->CopyBufferRegion(data_.get(), 0, retval.get(), 0, size);
        transition_resource(
            command_list, data_.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
    }

    return retval;
}

void sphere_rt_data::clear() {
    _bbox = {glm::vec3(0.0f), glm::vec3(0.0f)};
    _max_radius = 0.0f;
    data_ = nullptr;
    _cnt_spheres = 0;
}

void sphere_rt_data::fit_bounding_box(const mmpld::list_header& header, const void* particles) {
    typedef std::decay<decltype(*header.bounding_box)>::type bbox_type;
    typedef std::numeric_limits<bbox_type> bbox_limits;

    assert(particles != nullptr);
    log::instance().write_line(log_level::verbose, "Recomputing bounding "
                                                   "box of MMPLD data from data actually contained in the active "
                                                   "particle list ...");

    auto cur = static_cast<const std::uint8_t*>(particles);
    const auto find_radius = (header.radius <= 0.0f);
    const auto stride = mmpld::get_stride<std::size_t>(header);

    // Initialise with extrema.
    for (glm::length_t i = 0; i < this->_bbox[0].length(); ++i) {
        this->_bbox[0][i] = (bbox_limits::max) ();
        this->_bbox[1][i] = (bbox_limits::lowest) ();
    }

    // Search minimum and maximum as well as maximum radius as necessary.
    this->_max_radius = find_radius ? (std::numeric_limits<decltype(header.radius)>::lowest)() : header.radius;
    for (std::size_t i = 0; i < header.particles; ++i, cur += stride) {
        const auto pos = reinterpret_cast<const float*>(cur);
        for (glm::length_t c = 0; c < 3; ++c) {
            if (pos[c] < this->_bbox[0][c]) {
                this->_bbox[0][c] = pos[c];
            }
            if (pos[c] > this->_bbox[1][c]) {
                this->_bbox[1][c] = pos[c];
            }
        }

        if (find_radius) {
            if (pos[4] > this->_max_radius) {
                this->_max_radius = pos[4];
            }
        }
    }

    // Account for the radius.
    for (glm::length_t i = 0; i < this->_bbox[0].length(); ++i) {
        this->_bbox[0][i] -= this->_max_radius;
        this->_bbox[1][i] += this->_max_radius;
    }

    log::instance().write_line(log_level::verbose,
        "Recomputed "
        "single-frame bounding box of MMPLD data is ({}, {}, {}) - "
        "({}, {}, {}) with maximum radius of {}.",
        this->_bbox[0][0], this->_bbox[0][1], this->_bbox[0][2], this->_bbox[1][0], this->_bbox[1][1],
        this->_bbox[1][2], this->_max_radius);
}
} // namespace trrojan::d3d12
