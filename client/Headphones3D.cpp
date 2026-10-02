// Real-time 3D headphone illustration.
//
// A model is a triangle mesh with a normal per vertex. A real one, converted from a 3D file with
// tooling/ConvertModel.py, is loaded from Models/ next to the executable when its file name appears
// in the product's name (Models/WH-1000XM5.mesh for a WH-1000XM5). Otherwise a model is generated
// here from parametric surfaces: a headband swept along an arch with a flat rounded section that
// narrows into round metal sliders, oval ear cups shaped as superellipsoids of revolution (flat
// faces, soft edges) and a torus for each ear cushion; or, for earbuds, two pebbles with silicone
// tips.
//
// A view is rendered in software into an image: the vertices are rotated, projected with a little
// perspective and lit in view space like a studio product shot (key, fill, rim light and a specular
// highlight), then the triangles are filled with a depth buffer at 3x3 samples per pixel, which
// gives the edges their anti-aliasing. The image goes into a texture that is drawn again for as long
// as the view stays within a fraction of a pixel of it, so a model at rest costs one textured quad
// per frame, and a turn renders a new image each frame: a few milliseconds even for 50k triangles.
#include "Headphones3D.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// Implemented by SDLMain.cpp: textures the client fills itself, RGBA with straight alpha.
ImTextureID clientTextureCreate(int width, int height);
void clientTextureUpdate(ImTextureID texture, const void* pixels, int width, int height, int pitch);
void clientTextureDestroy(ImTextureID texture);

namespace Headphones3D
{
namespace
{
    constexpr float kPi = 3.14159265358979f;
    constexpr float kCamera = 5.5f; // Camera distance in model units: just enough perspective
    constexpr int kSamples = 3;     // Samples per pixel along each axis

    struct V3
    {
        float x = 0.0f, y = 0.0f, z = 0.0f;
    };
    V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
    V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
    V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
    float Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    V3 Cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
    V3 Normalize(V3 a)
    {
        const float length = std::sqrt(Dot(a, a));
        return length > 1e-8f ? a * (1.0f / length) : V3{0.0f, 0.0f, -1.0f};
    }

    float SignPow(float v, float e) { return std::copysign(std::pow(std::fabs(v), e), v); }

    enum Material : unsigned char
    {
        kShell,   // Satin plastic: headband top, cup sides
        kFace,    // The flat outer face of a cup, a touch more matte
        kCushion, // Soft leather: ear pads
        kFabric,  // The cloth inside the cushion ring
        kMetal,   // Sliders
        kTip,     // Silicone ear tips
        kBand,    // Headband padding: the shell's colour, less glossy
        kTrim,    // Small parts: buttons, grilles, seams
        kAccent,  // A part that keeps its own colour (Part::own)
        kMaterialCount
    };

    // How the vertices of one part of a model are shaded.
    struct Part
    {
        Material material;
        float own[3]; // The colour of a kAccent part
    };

    struct Vertex
    {
        V3 p, n;
        unsigned char part; // Index into Mesh::parts
    };

    // While a model is generated: a grid of nu x nv vertices stored row by row and joined into
    // quads; a wrapping direction joins its last row (or column) back to the first.
    struct Grid
    {
        int first, nu, nv;
        bool wrapU, wrapV;
    };

    struct Mesh
    {
        std::vector<Vertex> vertices;  // Centred on the model's bounding box
        std::vector<uint32_t> indices; // Three per triangle, counter-clockwise from the side it faces
        std::vector<Part> parts;
        std::vector<Grid> grids;
        float halfHeight = 1.0f;
        float aspect = 1.0f;
    };

    void BeginGrid(Mesh& mesh, int nu, int nv, bool wrapU, bool wrapV)
    {
        mesh.grids.push_back({static_cast<int>(mesh.vertices.size()), nu, nv, wrapU, wrapV});
    }

    // A superellipsoid of revolution about `axis`, stretched to an oval by ry (along `up`) and rz.
    // The profile runs from the pole on the +axis side (the outer face) to the opposite one.
    void AddPuck(Mesh& mesh, V3 center, V3 axis, V3 up, float depth, float ry, float rz, float e,
                 Material outer, Material side, Material inner, float innerFraction, int nu = 18, int nv = 30)
    {
        const V3 across = Normalize(Cross(axis, up));
        const float a = depth * 0.5f;
        BeginGrid(mesh, nu, nv, false, true);
        for (int i = 0; i < nu; ++i)
        {
            const float t = kPi * static_cast<float>(i) / static_cast<float>(nu - 1);
            const float c = std::cos(t), s = std::sin(t);
            const float x = a * SignPow(c, e), r = SignPow(s, e);
            // Gradient of |x/a|^(2/e) + |r|^(2/e): the analytic normal, exact at the poles too.
            const float nx = SignPow(c, 2.0f - e) / a, nr = SignPow(s, 2.0f - e);
            Material m = side;
            if (x > a * 0.55f)
                m = outer;
            else if (x < -a * 0.55f && r < innerFraction)
                m = inner;
            for (int j = 0; j < nv; ++j)
            {
                const float phi = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(nv);
                const float sp = std::sin(phi), cp = std::cos(phi);
                const V3 p = center + axis * x + up * (ry * r * sp) + across * (rz * r * cp);
                const V3 n = Normalize(axis * nx + up * (nr * sp / ry) + across * (nr * cp / rz));
                mesh.vertices.push_back({p, n, m});
            }
        }
    }

    // An oval torus about `axis`: an ear cushion.
    void AddCushion(Mesh& mesh, V3 center, V3 axis, V3 up, float ry, float rz, float tubeAxial, float tubeRadial,
                    Material m, int nu = 32, int nv = 10)
    {
        const V3 across = Normalize(Cross(axis, up));
        BeginGrid(mesh, nu, nv, true, true);
        for (int i = 0; i < nu; ++i)
        {
            const float phi = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(nu);
            const float sp = std::sin(phi), cp = std::cos(phi);
            const V3 ring = center + up * (ry * sp) + across * (rz * cp);
            const V3 outward = Normalize(up * (sp / ry) + across * (cp / rz));
            for (int j = 0; j < nv; ++j)
            {
                const float psi = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(nv);
                const float c = std::cos(psi), s = std::sin(psi);
                const V3 p = ring + outward * (tubeRadial * c) + axis * (tubeAxial * s);
                const V3 n = Normalize(outward * (c / tubeRadial) + axis * (s / tubeAxial));
                mesh.vertices.push_back({p, n, m});
            }
        }
    }

    // One cross-section of a swept tube. The section is a superellipse: `halfN` across the
    // thickness (along the in-plane normal of the path), `halfB` across the width (along Z).
    struct Ring
    {
        V3 c, t;
        float halfN, halfB, e;
        bool metal;
    };

    void AddTube(Mesh& mesh, const std::vector<Ring>& rings, int nv = 14)
    {
        const V3 binormal{0.0f, 0.0f, 1.0f};
        BeginGrid(mesh, static_cast<int>(rings.size()), nv, false, true);
        for (const Ring& ring : rings)
        {
            const V3 n = Normalize(Cross(binormal, ring.t)); // Out of the arch
            const float hn = std::max(ring.halfN, 1e-4f), hb = std::max(ring.halfB, 1e-4f);
            for (int j = 0; j < nv; ++j)
            {
                const float phi = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(nv);
                const float c = std::cos(phi), s = std::sin(phi);
                const V3 p = ring.c + n * (ring.halfN * SignPow(c, ring.e)) + binormal * (ring.halfB * SignPow(s, ring.e));
                const V3 normal = Normalize(n * (SignPow(c, 2.0f - ring.e) / hn) + binormal * (SignPow(s, 2.0f - ring.e) / hb));
                // The underside of the band (towards the head) is padded.
                const Material m = ring.metal ? kMetal : (c < -0.35f ? kCushion : kShell);
                mesh.vertices.push_back({p, normal, m});
            }
        }
    }

    float Smooth(float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    Mesh BuildOverEar()
    {
        Mesh mesh;
        constexpr float W = 1.0f;          // Half the distance between the cups
        constexpr float kArchHeight = 1.1f;
        constexpr float kSliderBottom = -0.24f;
        constexpr float kCupY = -0.7f;
        constexpr float kSliderRadius = 0.034f;
        constexpr float kBandHalfThickness = 0.046f, kBandHalfWidth = 0.15f;

        // Headband and sliders: one tube from inside the left cup, over the top, into the right one.
        std::vector<Ring> rings;
        constexpr int kSliderRings = 6;
        for (int i = 0; i < kSliderRings; ++i)
        {
            const float f = static_cast<float>(i) / kSliderRings; // Stops short of the arch start
            const float y = (kSliderBottom - 0.1f) + (0.1f - kSliderBottom) * f;
            const float r = i == 0 ? 0.0f : kSliderRadius; // Closed end, hidden inside the cup
            rings.push_back({{-W, y, 0.0f}, {0.0f, 1.0f, 0.0f}, r, r, 1.0f, true});
        }
        auto arch = [&](float u) -> V3
        {
            const float a = kPi * (1.0f - u);
            return {W * SignPow(std::cos(a), 0.9f), kArchHeight * SignPow(std::sin(a), 0.7f), 0.0f};
        };
        constexpr int kArchRings = 44;
        for (int i = 0; i <= kArchRings; ++i)
        {
            const float u = static_cast<float>(i) / kArchRings;
            const V3 c = arch(u);
            V3 t = Normalize(arch(std::min(u + 0.002f, 1.0f)) - arch(std::max(u - 0.002f, 0.0f)));
            if (i == 0)
                t = {0.0f, 1.0f, 0.0f};
            else if (i == kArchRings)
                t = {0.0f, -1.0f, 0.0f};
            // The round slider widens into the flat band over the first and last stretch.
            const float blend = Smooth(std::min(u, 1.0f - u) / 0.14f);
            rings.push_back({c, t,
                             kSliderRadius + (kBandHalfThickness - kSliderRadius) * blend,
                             kSliderRadius + (kBandHalfWidth - kSliderRadius) * blend,
                             1.0f + (0.42f - 1.0f) * blend, blend < 0.3f});
        }
        for (int i = 1; i <= kSliderRings; ++i)
        {
            const float f = static_cast<float>(i) / kSliderRings;
            const float y = (0.0f) + (kSliderBottom - 0.1f) * f;
            const float r = i == kSliderRings ? 0.0f : kSliderRadius;
            rings.push_back({{W, y, 0.0f}, {0.0f, -1.0f, 0.0f}, r, r, 1.0f, true});
        }
        AddTube(mesh, rings);

        // Ear cups with their cushions, facing each other.
        for (float side : {-1.0f, 1.0f})
        {
            const V3 axis{side, 0.0f, 0.0f}; // Outward
            const V3 up{0.0f, 1.0f, 0.0f};
            const V3 cup{side * W, kCupY, 0.0f};
            constexpr float kDepth = 0.34f;
            AddPuck(mesh, cup, axis, up, kDepth, 0.5f, 0.41f, 0.34f, kFace, kShell, kFabric, 0.74f);
            AddCushion(mesh, cup - axis * (kDepth * 0.5f + 0.045f), axis, up, 0.39f, 0.3f, 0.075f, 0.1f, kCushion);
        }
        return mesh;
    }

    Mesh BuildEarbuds()
    {
        Mesh mesh;
        for (float side : {-1.0f, 1.0f})
        {
            // Outer faces turned partly towards the viewer, the way they sit in a product shot.
            const V3 axis = Normalize({side * 0.45f, 0.12f, -0.88f});
            const V3 up{0.0f, 1.0f, 0.0f};
            const V3 body{side * 0.7f, 0.0f, 0.0f};
            constexpr float kDepth = 0.44f;
            AddPuck(mesh, body, axis, up, kDepth, 0.46f, 0.46f, 0.46f, kFace, kShell, kShell, 0.0f, 18, 28);
            AddPuck(mesh, body - axis * (kDepth * 0.5f + 0.09f) + V3{-side * 0.06f, -0.08f, 0.0f}, axis, up,
                    0.22f, 0.19f, 0.19f, 0.62f, kTip, kTip, kTip, 0.0f, 12, 20);
        }
        return mesh;
    }

    // Centres the model on its bounding box, so that it turns about its middle, and measures it.
    void Centre(Mesh& mesh)
    {
        V3 lo{FLT_MAX, FLT_MAX, FLT_MAX}, hi{-FLT_MAX, -FLT_MAX, -FLT_MAX};
        for (const Vertex& v : mesh.vertices)
        {
            lo = {std::min(lo.x, v.p.x), std::min(lo.y, v.p.y), std::min(lo.z, v.p.z)};
            hi = {std::max(hi.x, v.p.x), std::max(hi.y, v.p.y), std::max(hi.z, v.p.z)};
        }
        const V3 centre = (lo + hi) * 0.5f;
        for (Vertex& v : mesh.vertices)
            v.p = v.p - centre;
        mesh.halfHeight = std::max(0.01f, (hi.y - lo.y) * 0.5f);
        mesh.aspect = (hi.x - lo.x) / std::max(0.01f, hi.y - lo.y);
    }

    // A generated model: its grids joined into triangles, each wound to face the way its vertex
    // normals point (a degenerate one, at the pole of a puck, is dropped).
    Mesh Finish(Mesh mesh)
    {
        for (const Grid& g : mesh.grids)
        {
            const int quadsU = g.wrapU ? g.nu : g.nu - 1, quadsV = g.wrapV ? g.nv : g.nv - 1;
            auto vertex = [&](int i, int j) { return static_cast<uint32_t>(g.first + (i % g.nu) * g.nv + (j % g.nv)); };
            auto triangle = [&](uint32_t a, uint32_t b, uint32_t c)
            {
                const Vertex &va = mesh.vertices[a], &vb = mesh.vertices[b], &vc = mesh.vertices[c];
                const V3 face = Cross(vb.p - va.p, vc.p - va.p);
                if (Dot(face, face) < 1e-14f)
                    return;
                if (Dot(face, va.n + vb.n + vc.n) < 0.0f)
                    std::swap(b, c);
                mesh.indices.insert(mesh.indices.end(), {a, b, c});
            };
            for (int i = 0; i < quadsU; ++i)
                for (int j = 0; j < quadsV; ++j)
                {
                    const uint32_t v[4] = {vertex(i, j), vertex(i + 1, j), vertex(i + 1, j + 1), vertex(i, j + 1)};
                    triangle(v[0], v[1], v[2]);
                    triangle(v[0], v[2], v[3]);
                }
        }
        mesh.grids.clear();
        for (int m = 0; m < kMaterialCount; ++m) // A generated vertex's part is its material
            mesh.parts.push_back({static_cast<Material>(m), {0.0f, 0.0f, 0.0f}});
        Centre(mesh);
        return mesh;
    }

    // Reads a model written by tooling/ConvertModel.py; its description there has the layout.
    bool LoadMesh(const char* path, Mesh& mesh)
    {
        size_t size = 0;
        void* file = SDL_LoadFile(path, &size);
        if (!file)
            return false;
        const unsigned char* at = static_cast<const unsigned char*>(file);
        const unsigned char* const end = at + size;
        auto read = [&](void* out, size_t bytes)
        {
            if (static_cast<size_t>(end - at) < bytes)
                return false;
            std::memcpy(out, at, bytes);
            at += bytes;
            return true;
        };
        bool loaded = false;
        do
        {
            char magic[4];
            uint32_t counts[4]; // Vertices, triangles, groups, index size
            float extent = 0.0f;
            if (!read(magic, sizeof(magic)) || std::memcmp(magic, "SHM1", 4) != 0 || !read(counts, sizeof(counts)) ||
                !read(&extent, sizeof(extent)))
                break;
            const uint32_t vertexCount = counts[0], triangleCount = counts[1], groupCount = counts[2], indexSize = counts[3];
            if (vertexCount == 0 || triangleCount == 0 || groupCount == 0 || groupCount > 255 ||
                (indexSize != 2 && indexSize != 4) || !(extent > 0.0f))
                break;
            // The roles the converter writes, in its order: body, cushion, band, trim, accent.
            static constexpr Material kRoles[] = {kShell, kCushion, kBand, kTrim, kAccent};
            std::vector<uint32_t> groupTriangles(groupCount);
            uint64_t total = 0;
            bool groups = true;
            for (uint32_t g = 0; g < groupCount && groups; ++g)
            {
                unsigned char info[4]; // Role, then the colour in the file
                groups = read(&groupTriangles[g], sizeof(uint32_t)) && read(info, sizeof(info));
                mesh.parts.push_back({info[0] < std::size(kRoles) ? kRoles[info[0]] : kShell,
                                      {info[1] / 255.0f, info[2] / 255.0f, info[3] / 255.0f}});
                total += groupTriangles[g];
            }
            if (!groups || total != triangleCount ||
                static_cast<uint64_t>(end - at) < vertexCount * 12ull + triangleCount * 3ull * indexSize)
                break;
            mesh.vertices.resize(vertexCount);
            const float toModel = extent / 32767.0f;
            for (Vertex& v : mesh.vertices)
            {
                int16_t q[6]; // Position, normal
                read(q, sizeof(q));
                v.p = {q[0] * toModel, q[1] * toModel, q[2] * toModel};
                v.n = Normalize({static_cast<float>(q[3]), static_cast<float>(q[4]), static_cast<float>(q[5])});
                v.part = 0;
            }
            mesh.indices.resize(static_cast<size_t>(triangleCount) * 3);
            bool indices = true;
            for (uint32_t& index : mesh.indices)
            {
                if (indexSize == 2)
                {
                    uint16_t narrow;
                    read(&narrow, sizeof(narrow));
                    index = narrow;
                }
                else
                    read(&index, sizeof(index));
                indices = indices && index < vertexCount;
            }
            if (!indices)
                break;
            // Triangles come group by group, and a vertex belongs to one group.
            size_t corner = 0;
            for (uint32_t g = 0; g < groupCount; ++g)
                for (size_t k = 0; k < static_cast<size_t>(groupTriangles[g]) * 3; ++k)
                    mesh.vertices[mesh.indices[corner++]].part = static_cast<unsigned char>(g);
            loaded = true;
        } while (false);
        SDL_free(file);
        if (loaded)
            Centre(mesh);
        return loaded;
    }

    struct NamedMesh
    {
        std::string name; // The file name without its extension: "WH-1000XM5"
        Mesh mesh;
    };

    // The real models in Models/ next to the executable, loaded on first use.
    const std::vector<std::unique_ptr<NamedMesh>>& Library()
    {
        static const std::vector<std::unique_ptr<NamedMesh>> library = []
        {
            std::vector<std::unique_ptr<NamedMesh>> models;
            const char* base = SDL_GetBasePath();
            if (!base)
                return models;
            const std::string folder = std::string(base) + "Models";
            int count = 0;
            char** files = SDL_GlobDirectory(folder.c_str(), "*.mesh", SDL_GLOB_CASEINSENSITIVE, &count);
            for (int i = 0; files && i < count; ++i)
            {
                std::string name = files[i];
                name = name.substr(name.find_last_of("/\\") + 1);
                auto model = std::make_unique<NamedMesh>();
                model->name = name.substr(0, name.rfind('.'));
                if (LoadMesh((folder + "/" + name).c_str(), model->mesh))
                    models.push_back(std::move(model));
                else
                    SDL_Log("Headphones3D: %s/%s is not a model this client can read", folder.c_str(), name.c_str());
            }
            SDL_free(files);
            return models;
        }();
        return library;
    }

    bool IsEarbuds(const char* product) { return product && SDL_strncasecmp(product, "WF-", 3) == 0; }

    bool Contains(const char* text, const std::string& word)
    {
        const size_t length = std::strlen(text);
        for (size_t i = 0; !word.empty() && i + word.size() <= length; ++i)
            if (SDL_strncasecmp(text + i, word.c_str(), word.size()) == 0)
                return true;
        return false;
    }

    const Mesh& MeshFor(const char* product)
    {
        if (product && *product)
            for (const auto& model : Library())
                if (Contains(product, model->name))
                    return model->mesh;
        static const Mesh overEar = Finish(BuildOverEar());
        static const Mesh earbuds = Finish(BuildEarbuds());
        return IsEarbuds(product) ? earbuds : overEar;
    }

    struct Rgb
    {
        float r, g, b;
    };
    Rgb operator*(Rgb a, float s) { return {a.r * s, a.g * s, a.b * s}; }
    Rgb Mix(Rgb a, Rgb b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }

    struct Look
    {
        Rgb albedo;
        float specular, shininess;
    };

    Look LookFor(const Part& part, Rgb base)
    {
        const float luma = 0.3f * base.r + 0.59f * base.g + 0.11f * base.b;
        const Rgb grey{luma, luma, luma};
        switch (part.material)
        {
        case kShell: return {base, 0.3f, 26.0f};
        case kFace: return {Mix(base, grey, 0.05f) * 0.97f, 0.2f, 16.0f};
        case kCushion: return {Mix(base, grey, 0.25f) * 0.62f, 0.07f, 8.0f};
        case kFabric: return {Mix(base, grey, 0.4f) * 0.3f, 0.02f, 4.0f};
        case kMetal: return {Mix(base, {0.8f, 0.8f, 0.82f}, 0.6f), 0.65f, 60.0f};
        case kTip: return {Mix(base, {0.2f, 0.2f, 0.22f}, 0.7f), 0.12f, 10.0f};
        case kBand: return {Mix(base, grey, 0.1f) * 0.9f, 0.14f, 12.0f};
        case kTrim: return {Mix(base, grey, 0.2f) * 0.55f, 0.12f, 12.0f};
        case kAccent: return {{part.own[0], part.own[1], part.own[2]}, 0.3f, 26.0f};
        default: return {base, 0.2f, 16.0f};
        }
    }

    // x^n for the highlight without pow: (1 - n(1 - x) / 8)^8 follows it closely near 1, the only
    // place where a highlight is bright enough to matter.
    float Highlight(float x, float n)
    {
        float t = std::max(0.0f, 1.0f - n * (1.0f - x) * 0.125f);
        t *= t;
        t *= t;
        return t * t;
    }

    // A rendered view: whole pixels around the model, placed relative to its centre.
    struct Image
    {
        int left = 0, top = 0; // The top left pixel's offset from the model's centre
        int width = 0, height = 0;
        std::vector<ImU32> pixels; // RGBA, straight alpha
        ImVec2 shadowCentre;       // From the model's centre, in pixels
        float shadowWidth = 0.0f;
    };

    // A vertex after the vertex stage: position in samples, 1 / distance (larger is nearer) and
    // its lit colour.
    struct Projected
    {
        float x, y, depth, r, g, b;
    };

    void Render(const Mesh& mesh, float yaw, float pitch, float size, ImU32 colour, Image& image)
    {
        // `size` is half the height; wide models (a pair of earbuds) are also kept within a width.
        const float scale = size / std::max(mesh.halfHeight, mesh.halfHeight * mesh.aspect / 1.15f);
        const float cy = std::cos(yaw), sy = std::sin(yaw);
        const float cp = std::cos(pitch), sp = std::sin(pitch);
        auto toView = [&](V3 v) -> V3
        {
            const V3 a{v.x * cy - v.z * sy, v.y, v.x * sy + v.z * cy}; // Yaw: +X swings away
            return {a.x, a.y * cp + a.z * sp, -a.y * sp + a.z * cp};    // Pitch: the top tips towards us
        };
        // Lights in view space. With this little perspective every vertex is seen from nearly
        // straight on, so the highlight and the rim use that one direction.
        const V3 key = Normalize({-0.55f, 0.72f, -0.55f});
        const V3 fill = Normalize({0.85f, 0.1f, -0.3f});
        const V3 halfway = Normalize(key + V3{0.0f, 0.0f, -1.0f});
        const ImVec4 baseF = ImGui::ColorConvertU32ToFloat4(colour);
        const Rgb base{baseF.x, baseF.y, baseF.z};
        std::vector<Look> looks;
        looks.reserve(mesh.parts.size());
        for (const Part& part : mesh.parts)
            looks.push_back(LookFor(part, base));

        // Vertex stage: rotate, project (in pixels from the centre) and light every vertex once.
        static std::vector<Projected> projected;
        projected.resize(mesh.vertices.size());
        float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
        for (size_t i = 0; i < mesh.vertices.size(); ++i)
        {
            const Vertex& v = mesh.vertices[i];
            const V3 p = toView(v.p);
            const V3 n = toView(v.n); // A rotation keeps it unit length
            const float distance = std::max(0.5f, kCamera + p.z);
            const float perspective = scale * kCamera / distance;
            Projected& out = projected[i];
            out.x = p.x * perspective;
            out.y = -p.y * perspective;
            out.depth = 1.0f / distance;
            minX = std::min(minX, out.x), maxX = std::max(maxX, out.x);
            minY = std::min(minY, out.y), maxY = std::max(maxY, out.y);
            const Look& look = looks[v.part];
            const float diffuse = 0.34f + 0.62f * std::max(0.0f, Dot(n, key)) + 0.2f * std::max(0.0f, Dot(n, fill));
            const float highlight = look.specular * Highlight(std::max(0.0f, Dot(n, halfway)), look.shininess);
            const float edge = 1.0f - std::max(0.0f, -n.z);
            const float rim = 0.18f * edge * edge * edge;
            out.r = std::clamp(look.albedo.r * diffuse + highlight + rim, 0.0f, 1.0f);
            out.g = std::clamp(look.albedo.g * diffuse + highlight + rim, 0.0f, 1.0f);
            out.b = std::clamp(look.albedo.b * diffuse + highlight + rim, 0.0f, 1.0f);
        }
        // Whole pixels around the projection, with one to spare on each side for the soft edge.
        image.left = static_cast<int>(std::floor(minX)) - 1;
        image.top = static_cast<int>(std::floor(minY)) - 1;
        image.width = static_cast<int>(std::ceil(maxX)) + 1 - image.left;
        image.height = static_cast<int>(std::ceil(maxY)) + 1 - image.top;
        image.shadowCentre = ImVec2((minX + maxX) * 0.5f, maxY + size * 0.02f);
        image.shadowWidth = (maxX - minX) * 0.46f;
        for (Projected& p : projected)
        {
            p.x = (p.x - static_cast<float>(image.left)) * kSamples;
            p.y = (p.y - static_cast<float>(image.top)) * kSamples;
        }

        // Triangle stage, on the sample grid. A triangle that is wound clockwise on screen faces
        // away; the depth test sorts out the rest.
        const int width = image.width * kSamples, height = image.height * kSamples;
        static std::vector<float> depth;
        static std::vector<ImU32> shade;
        depth.assign(static_cast<size_t>(width) * height, 0.0f);
        shade.resize(depth.size());
        for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3)
        {
            const Projected& a = projected[mesh.indices[t]];
            const Projected& b = projected[mesh.indices[t + 1]];
            const Projected& c = projected[mesh.indices[t + 2]];
            const float area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
            if (area <= 0.0f)
                continue;
            const int x0 = std::max(0, static_cast<int>(std::min({a.x, b.x, c.x})));
            const int x1 = std::min(width - 1, static_cast<int>(std::max({a.x, b.x, c.x})));
            const int y0 = std::max(0, static_cast<int>(std::min({a.y, b.y, c.y})));
            const int y1 = std::min(height - 1, static_cast<int>(std::max({a.y, b.y, c.y})));
            if (x0 > x1 || y0 > y1)
                continue;
            // Edge functions at the first sample's centre (each is the weight of the opposite
            // corner, times the area), and how they change per sample along x and y.
            const float inverse = 1.0f / area;
            const float px = static_cast<float>(x0) + 0.5f, py = static_cast<float>(y0) + 0.5f;
            float row0 = (c.x - b.x) * (py - b.y) - (c.y - b.y) * (px - b.x);
            float row1 = (a.x - c.x) * (py - c.y) - (a.y - c.y) * (px - c.x);
            float row2 = (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
            const float stepX0 = b.y - c.y, stepX1 = c.y - a.y, stepX2 = a.y - b.y;
            const float stepY0 = c.x - b.x, stepY1 = a.x - c.x, stepY2 = b.x - a.x;
            for (int y = y0; y <= y1; ++y, row0 += stepY0, row1 += stepY1, row2 += stepY2)
            {
                float e0 = row0, e1 = row1, e2 = row2;
                float* depthRow = &depth[static_cast<size_t>(y) * width];
                ImU32* shadeRow = &shade[static_cast<size_t>(y) * width];
                for (int x = x0; x <= x1; ++x, e0 += stepX0, e1 += stepX1, e2 += stepX2)
                {
                    if (e0 < 0.0f || e1 < 0.0f || e2 < 0.0f)
                        continue;
                    const float w0 = e0 * inverse, w1 = e1 * inverse, w2 = e2 * inverse;
                    const float z = w0 * a.depth + w1 * b.depth + w2 * c.depth;
                    if (z <= depthRow[x])
                        continue;
                    depthRow[x] = z;
                    auto channel = [&](float va, float vb, float vc)
                    { return static_cast<ImU32>((w0 * va + w1 * vb + w2 * vc) * 255.0f + 0.5f); };
                    shadeRow[x] = channel(a.r, b.r, c.r) | channel(a.g, b.g, c.g) << 8 | channel(a.b, b.b, c.b) << 16;
                }
            }
        }

        // Resolve: each pixel is the average of its covered samples, and as opaque as it is covered.
        image.pixels.resize(static_cast<size_t>(image.width) * image.height);
        constexpr unsigned kPerPixel = kSamples * kSamples;
        for (int y = 0; y < image.height; ++y)
            for (int x = 0; x < image.width; ++x)
            {
                unsigned r = 0, g = 0, b = 0, covered = 0;
                for (int j = 0; j < kSamples; ++j)
                {
                    const size_t row = static_cast<size_t>(y * kSamples + j) * width + static_cast<size_t>(x) * kSamples;
                    for (int i = 0; i < kSamples; ++i)
                        if (depth[row + i] > 0.0f)
                        {
                            const ImU32 s = shade[row + i];
                            r += s & 0xFF, g += (s >> 8) & 0xFF, b += (s >> 16) & 0xFF, ++covered;
                        }
                }
                image.pixels[static_cast<size_t>(y) * image.width + x] =
                    covered == 0 ? 0 : IM_COL32(r / covered, g / covered, b / covered, covered * 255 / kPerPixel);
            }
    }

    // A view kept in a texture, and what it shows.
    struct Frame
    {
        const Mesh* mesh = nullptr;
        float yaw = 0.0f, pitch = 0.0f, size = 0.0f; // Size in framebuffer pixels
        ImU32 colour = 0;
        unsigned lastUse = 0;
        Image image;
        ImTextureID texture = ImTextureID_Invalid;
        int textureWidth = 0, textureHeight = 0;
    };

    bool Shows(const Frame& frame, const Mesh* mesh, float yaw, float pitch, float size, ImU32 colour)
    {
        // The model's outermost points are about `size` pixels from its axis, so these steps keep
        // every vertex within a third of a pixel of where a fresh render would put it.
        const float angle = 0.3f / size;
        return frame.mesh == mesh && frame.colour == colour && std::fabs(frame.yaw - yaw) < angle &&
               std::fabs(frame.pitch - pitch) < angle && std::fabs(frame.size - size) < 0.25f;
    }
}

float Aspect(const char* product)
{
    return MeshFor(product).aspect;
}

float RestYaw(const char* product)
{
    return IsEarbuds(product) ? 0.2f : 0.62f;
}

void Draw(ImDrawList* draw, ImVec2 center, const View& view, const char* product, ImU32 colour, float alpha)
{
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    if (alpha <= 0.002f || view.size <= 1.0f)
        return;
    // Rendered in framebuffer pixels, so it stays sharp on a high-density display.
    const float density = ImGui::GetIO().DisplayFramebufferScale.x > 0.0f ? ImGui::GetIO().DisplayFramebufferScale.x : 1.0f;
    const float size = view.size * density;
    const Mesh* mesh = &MeshFor(product);

    // A few views are kept (the hero, the connection sheet over it, ...); the least recently used
    // one gives way to a new view.
    static Frame frames[4];
    static unsigned uses = 0;
    Frame* frame = nullptr;
    for (Frame& f : frames)
        if (Shows(f, mesh, view.yaw, view.pitch, size, colour))
        {
            frame = &f;
            break;
        }
    if (!frame)
    {
        frame = &frames[0];
        for (Frame& f : frames)
            if (f.lastUse < frame->lastUse)
                frame = &f;
        frame->mesh = mesh;
        frame->yaw = view.yaw;
        frame->pitch = view.pitch;
        frame->size = size;
        frame->colour = colour;
        Render(*mesh, view.yaw, view.pitch, size, colour, frame->image);
        const Image& image = frame->image;
        if (image.width > frame->textureWidth || image.height > frame->textureHeight)
        {
            if (frame->texture != ImTextureID_Invalid)
                clientTextureDestroy(frame->texture);
            // Room to grow, so that a model scaling up as it appears is not reallocated each frame.
            frame->textureWidth = (image.width + 63) / 64 * 64;
            frame->textureHeight = (image.height + 63) / 64 * 64;
            frame->texture = clientTextureCreate(frame->textureWidth, frame->textureHeight);
        }
        if (frame->texture != ImTextureID_Invalid)
            clientTextureUpdate(frame->texture, image.pixels.data(), image.width, image.height, image.width * 4);
    }
    frame->lastUse = ++uses;
    if (frame->texture == ImTextureID_Invalid)
        return;

    // The image lands on whole pixels, texel for pixel.
    const Image& image = frame->image;
    const float cx = std::round(center.x * density), cy = std::round(center.y * density);
    // A soft contact shadow grounds the model, just under its lowest projected point.
    const ImVec2 shadow((cx + image.shadowCentre.x) / density, (cy + image.shadowCentre.y) / density);
    const float shadowWidth = image.shadowWidth / density;
    draw->AddEllipseFilled(shadow, {shadowWidth, view.size * 0.09f}, IM_COL32(0, 0, 0, static_cast<int>(22 * alpha)), 0.0f, 48);
    draw->AddEllipseFilled(shadow, {shadowWidth * 0.62f, view.size * 0.05f}, IM_COL32(0, 0, 0, static_cast<int>(28 * alpha)), 0.0f, 40);
    const ImVec2 min((cx + static_cast<float>(image.left)) / density, (cy + static_cast<float>(image.top)) / density);
    const ImVec2 max(min.x + static_cast<float>(image.width) / density, min.y + static_cast<float>(image.height) / density);
    draw->AddImage(ImTextureRef(frame->texture), min, max, ImVec2(0.0f, 0.0f),
                   ImVec2(static_cast<float>(image.width) / static_cast<float>(frame->textureWidth),
                          static_cast<float>(image.height) / static_cast<float>(frame->textureHeight)),
                   IM_COL32(255, 255, 255, static_cast<int>(alpha * 255.0f + 0.5f)));
}
}
