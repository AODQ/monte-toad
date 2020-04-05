/// Distributed under the MIT license:
///
/// Copyright (C) 2020 Arsène Pérard-Gayot
///
/// Permission is hereby granted, free of charge, to any person obtaining a
/// copy of this software and associated documentation files (the "Software"), to
/// deal in the Software without restriction, including without limitation the
/// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
/// sell copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in
/// all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
/// SOFTWARE.

#ifndef BVH_HPP
#define BVH_HPP

#include <glm/glm.hpp>

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <cstring>
#include <cmath>
#include <climits>

#include <algorithm>
#include <numeric>
#include <optional>
#include <memory>
#include <limits>
#include <array>
#include <stack>
#include <atomic>

namespace bvh {

#ifdef BVH_DOUBLE
using Scalar = double;
#else
using Scalar = float;
#endif

template <typename To, typename From>
To as(From from) {
    To to;
    std::memcpy(&to, &from, sizeof(from));
    return to;
}

/// A ray, defined by an origin and a direction, with minimum and maximum distances along the direction from the origin.
struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
    Scalar tmin;
    Scalar tmax;

    Ray() = default;
    Ray(const glm::vec3& origin, const glm::vec3& direction, Scalar tmin = Scalar(0), Scalar tmax = std::numeric_limits<Scalar>::max())
        : origin(origin), direction(direction), tmin(tmin), tmax(tmax)
    {}
};

/// A bounding box, represented with two extreme points.
struct BBox {
    glm::vec3 min, max;

    BBox() = default;
    BBox(const glm::vec3& v) : min(v), max(v) {}
    BBox(const glm::vec3& min, const glm::vec3& max) : min(min), max(max) {}

    BBox& extend(const BBox& bbox) {
        min.x = std::min(min.x, bbox.min.x);
        min.y = std::min(min.y, bbox.min.y);
        min.z = std::min(min.z, bbox.min.z);
        max.x = std::max(max.x, bbox.max.x);
        max.y = std::max(max.y, bbox.max.y);
        max.z = std::max(max.z, bbox.max.z);
        return *this;
    }

    BBox& extend(const glm::vec3& v) {
        return extend(BBox(v));
    }

    glm::vec3 diagonal() const {
        return max - min;
    }

    float half_area() const {
        glm::vec3 d = diagonal();
        return (d.x + d.y) * d.z + d.x * d.y;
    }

    static BBox full() {
        return BBox(glm::vec3(-std::numeric_limits<Scalar>::max()), glm::vec3(std::numeric_limits<Scalar>::max()));
    }

    static BBox empty() {
        return BBox(glm::vec3(std::numeric_limits<Scalar>::max()), glm::vec3(-std::numeric_limits<Scalar>::max()));
    }
};

#pragma omp declare reduction \
    (bbox_extend:BBox:omp_out.extend(omp_in)) \
    initializer(omp_priv = BBox::empty())

inline float multiply_add(float x, float y, float z) {
#ifdef FP_FAST_FMAF
    return std::fmaf(x, y, z);
#else
    return x * y + z;
#endif
}

inline double multiply_add(double x, double y, double z) {
#ifdef FP_FAST_FMA
    return std::fma(x, y, z);
#else
    return x * y + z;
#endif
}

inline glm::vec3 multiply_add(glm::vec3 x, glm::vec3 y, glm::vec3 z) {
    return glm::vec3(
        multiply_add(x.x, y.x, z.x),
        multiply_add(x.y, y.y, z.y),
        multiply_add(x.z, y.z, z.z)
    );
}

template <typename T>
void atomic_max(std::atomic<T>& x, T y) {
    T z = x.load();
    while (z < y && !x.compare_exchange_weak(z, y)) ;
}

/// This structure represents a BVH with a list of nodes and primitives indices.
/// This API is very low level but offers full control of the algorithm, including
/// the number of bins used during building or the maximum tree depth.
/// For a higher-level API, use the Accel structure.
template <size_t BinCount = 32, size_t MaxDepth = 64, size_t ParallelThreshold = 1024>
struct BVH {
    // The size of this structure is 32 bytes in single precision and 64 bytes in double precision.
    struct Node {
#ifdef BVH_DOUBLE
        using Index = uint64_t;
#else
        using Index = uint32_t;
#endif
        BBox bbox;
        bool is_leaf : 1;
        Index primitive_count : sizeof(Index) * CHAR_BIT - 1;
        Index first_child_or_primitive;

        std::pair<Scalar, Scalar> intersect(const glm::vec3& inverse_origin, const glm::vec3& inverse_direction, Scalar tmin, Scalar tmax, int ix, int iy, int iz) const {
            auto values = reinterpret_cast<const float*>(this);
            Scalar entry_x = multiply_add(values[    ix], inverse_direction.x, inverse_origin.x);
            Scalar entry_y = multiply_add(values[    iy], inverse_direction.y, inverse_origin.y);
            Scalar entry_z = multiply_add(values[    iz], inverse_direction.z, inverse_origin.z);
            Scalar exit_x  = multiply_add(values[3 - ix], inverse_direction.x, inverse_origin.x);
            Scalar exit_y  = multiply_add(values[5 - iy], inverse_direction.y, inverse_origin.y);
            Scalar exit_z  = multiply_add(values[7 - iz], inverse_direction.z, inverse_origin.z);
            return std::make_pair(
                std::max(std::max(entry_x, entry_y), std::max(entry_z, tmin)),
                std::min(std::min(exit_x,  exit_y),  std::min(exit_z,  tmax))
            );
        }
    };

    struct Bin {
        BBox   bbox;
        size_t primitive_count;
        Scalar right_cost;
    };

    static constexpr size_t bin_count = BinCount;
    static constexpr size_t max_depth = MaxDepth;

    static constexpr size_t parallel_threshold = ParallelThreshold;

    struct BuildTask {
        struct WorkItem {
            size_t node_index;
            size_t begin;
            size_t end;
            size_t depth;

            WorkItem() = default;
            WorkItem(size_t node_index, size_t begin, size_t end, size_t depth)
                : node_index(node_index), begin(begin), end(end), depth(depth)
            {}

            size_t work_size() const { return end - begin; }
        };

        BVH* bvh;

        const BBox* bboxes;
        const glm::vec3* centers;

        BuildTask(BVH* bvh, const BBox* bboxes, const glm::vec3* centers)
            : bvh(bvh), bboxes(bboxes), centers(centers)
        {}

        std::optional<std::pair<WorkItem, WorkItem>> build(const WorkItem& item) {
            Node& node = bvh->nodes[item.node_index];
            auto make_leaf = [] (Node& node, size_t begin, size_t end) {
                node.first_child_or_primitive = begin;
                node.primitive_count          = end - begin;
                node.is_leaf                  = true;
            };

            if (item.work_size() <= 1 || item.depth >= max_depth) {
                make_leaf(node, item.begin, item.end);
                return std::nullopt;
            }

            auto primitive_indices = bvh->primitive_indices.get();

            // Compute the bounding box of the centers of the primitives in this node
            BBox center_bbox = BBox::empty();
            for (size_t i = item.begin; i < item.end; ++i)
                center_bbox.extend(centers[primitive_indices[i]]);

            size_t best_split[3] = { 0, 0, 0 };
            Scalar best_cost[3] = {
                std::numeric_limits<Scalar>::max(),
                std::numeric_limits<Scalar>::max(),
                std::numeric_limits<Scalar>::max()
            };
            std::array<Bin, bin_count> bins_per_axis[3];

            auto inverse = glm::vec3(bin_count) * (glm::vec3(1.0f)/center_bbox.diagonal());
            auto base    = -center_bbox.min * inverse;
            auto bin_index = [=] (const glm::vec3& center, int axis) {
                return std::min(size_t(center[axis] * inverse[axis] + base[axis]), size_t(bin_count - 1));
            };

            #pragma omp taskloop if (item.work_size() > parallel_threshold) grainsize(1) default(shared)
            for (int axis = 0; axis < 3; ++axis) {
                auto& bins = bins_per_axis[axis];

                // Setup bins
                for (auto& bin : bins) {
                    bin.bbox = BBox::empty();
                    bin.primitive_count = 0;
                }

                // Fill bins
                for (size_t i = item.begin; i < item.end; ++i) {
                    size_t primitive_index = primitive_indices[i];
                    Bin& bin = bins[bin_index(centers[primitive_index], axis)];
                    bin.primitive_count++;
                    bin.bbox.extend(bboxes[primitive_index]);
                }

                // Right sweep to compute partial SAH
                BBox   current_bbox  = BBox::empty();
                size_t current_count = 0;
                for (size_t i = bin_count - 1; i > 0; --i) {
                    current_bbox.extend(bins[i].bbox);
                    current_count += bins[i].primitive_count;
                    bins[i].right_cost = current_bbox.half_area() * current_count;
                }

                // Left sweep to compute full cost and find minimum
                current_bbox  = BBox::empty();
                current_count = 0;
                for (size_t i = 0; i < bin_count - 1; ++i) {
                    current_bbox.extend(bins[i].bbox);
                    current_count += bins[i].primitive_count;
                    float cost = current_bbox.half_area() * current_count + bins[i + 1].right_cost;
                    if (cost < best_cost[axis]) {
                        best_split[axis] = i + 1;
                        best_cost[axis]  = cost;
                    }
                }
            }

            int best_axis = 0;
            if (best_cost[0] > best_cost[1])
                best_axis = 1;
            if (best_cost[best_axis] > best_cost[2])
                best_axis = 2;

            size_t total_primitives = item.end - item.begin;
            float  half_total_area  = node.bbox.half_area();

            // Check that the split is useful
            if (best_split[best_axis] != 0 && best_cost[best_axis] + bvh->traversal_cost * half_total_area < total_primitives * half_total_area) {
                // Split primitives according to split position
                size_t begin_right = std::partition(primitive_indices + item.begin, primitive_indices + item.end, [&] (size_t i) {
                    return bin_index(centers[i], best_axis) < best_split[best_axis];
                }) - primitive_indices;

                // Check that the split does not leave one side empty
                if (begin_right > item.begin && begin_right < item.end) {
                    // Allocate two nodes
                    size_t left_index;
                    #pragma omp atomic capture
                    { left_index = bvh->node_count; bvh->node_count += 2; }
                    Node& left  = bvh->nodes[left_index + 0];
                    Node& right = bvh->nodes[left_index + 1];
                    node.first_child_or_primitive = left_index;
                    node.primitive_count          = 0;
                    node.is_leaf                  = false;
                    
                    // Compute the bounding boxes of each node
                    auto& bins = bins_per_axis[best_axis];
                    left.bbox = BBox::empty();
                    for (size_t i = 0; i < best_split[best_axis]; ++i)
                        left.bbox.extend(bins[i].bbox);
                    right.bbox = BBox::empty();
                    for (size_t i = best_split[best_axis]; i < bin_count; ++i)
                        right.bbox.extend(bins[i].bbox);

                    // Return new work items
                    WorkItem first_item (left_index + 0, item.begin, begin_right, item.depth + 1);
                    WorkItem second_item(left_index + 1, begin_right, item.end,   item.depth + 1);
                    return std::make_optional(std::make_pair(first_item, second_item));
                }
            }

            make_leaf(node, item.begin, item.end);
            return std::nullopt;
        }

        template <typename... Args>
        void run(Args&&... args) {
            std::stack<WorkItem> stack;
            stack.emplace(std::forward<Args&&>(args)...);
            while (!stack.empty()) {
                auto work_item = stack.top();
                stack.pop();

                auto more_work = build(work_item);
                if (more_work) {
                    auto [first_item, second_item] = *more_work;
                    if (first_item.work_size() > second_item.work_size())
                        std::swap(first_item, second_item);

                    stack.push(second_item);
                    if (first_item.work_size() > parallel_threshold) {
                        BuildTask task(*this);
                        #pragma omp task firstprivate(task)
                        { task.run(first_item); }
                    } else {
                        stack.push(first_item);
                    }
                }
            }
        }
    };

    void build(const BBox* bboxes, const glm::vec3* centers, size_t primitive_count) {
        // Allocate buffers
        nodes.reset(new Node[2 * primitive_count + 1]);
        primitive_indices.reset(new size_t[primitive_count]);

        // Initialize root node
        BBox& root_bbox = nodes[0].bbox;
        root_bbox = BBox::empty();
        node_count = 1;

        #pragma omp parallel
        {
            #pragma omp for reduction(bbox_extend: root_bbox)
            for (size_t i = 0; i < primitive_count; ++i) {
                root_bbox.extend(bboxes[i]);
                primitive_indices[i] = i;
            }

            #pragma omp single
            { BuildTask(this, bboxes, centers).run(0, 0, primitive_count, 0); }
        }
    }

    struct Optimizer {
        BVH* bvh;

        std::unique_ptr<size_t[]> parents;

        Optimizer(BVH* bvh)
            : bvh(bvh), parents(new size_t[bvh->node_count])
        {
            parents[0] = std::numeric_limits<size_t>::max();
            #pragma omp parallel for
            for (size_t i = 0; i < bvh->node_count; ++i) {
                if (bvh->nodes[i].is_leaf)
                    continue;
                auto first_child = bvh->nodes[i].first_child_or_primitive;
                parents[first_child + 0] = i;
                parents[first_child + 1] = i;
            }
        }

        Scalar cost() {
            Scalar cost(0);
            #pragma omp parallel for reduction(+: cost)
            for (size_t i = 0; i < bvh->node_count; ++i) {
                if (bvh->nodes[i].is_leaf)
                    cost += bvh->nodes[i].bbox.half_area() * bvh->nodes[i].primitive_count;
                else
                    cost += bvh->traversal_cost * bvh->nodes[i].bbox.half_area();
            }
            return cost;
        }

        void refit(size_t child) {
            BBox bbox = bvh->nodes[child].bbox;
            while (child != 0) {
                auto parent = parents[child];
                bvh->nodes[parent].bbox = bbox.extend(bvh->nodes[sibling(child)].bbox);
                child = parent;
            }
        }

        std::array<size_t, 6> conflicts(size_t in, size_t out) {
            auto parent_in = parents[in];
            return std::array<size_t, 6> {
                in,
                sibling(in),
                parent_in,
                parent_in == 0 ? in : parents[parent_in],
                out,
                out == 0 ? out : parents[out],
            };
        }

        void reinsert(size_t in, size_t out) {
            auto sibling_in   = sibling(in);
            auto parent_in    = parents[in];
            auto sibling_node = bvh->nodes[sibling_in];
            auto out_node     = bvh->nodes[out];

            // Re-insert it into the destination
            bvh->nodes[out].bbox.extend(bvh->nodes[in].bbox);
            bvh->nodes[out].first_child_or_primitive = std::min(in, sibling_in);
            bvh->nodes[out].is_leaf = false;
            bvh->nodes[sibling_in] = out_node;
            bvh->nodes[parent_in] = sibling_node;

            // Update parent-child indices
            if (!out_node.is_leaf) {
                parents[out_node.first_child_or_primitive + 0] = sibling_in;
                parents[out_node.first_child_or_primitive + 1] = sibling_in;
            }
            if (!sibling_node.is_leaf) {
                parents[sibling_node.first_child_or_primitive + 0] = parent_in;
                parents[sibling_node.first_child_or_primitive + 1] = parent_in;
            }
            parents[sibling_in] = out;
            parents[in] = out;
        }

        size_t sibling(size_t index) const {
            assert(index != 0);
            return index % 2 == 1 ? index + 1 : index - 1;
        }

        using Insertion = std::pair<size_t, Scalar>;

        Insertion search(size_t in) {
            bool   down  = true;
            size_t pivot = parents[in];
            size_t out   = sibling(in);
            size_t out_best = out;

            auto bbox_in = bvh->nodes[in].bbox;
            auto bbox_parent = bvh->nodes[pivot].bbox;
            auto bbox_pivot = BBox::empty();

            Scalar d = 0;
            Scalar d_best = 0;
            const Scalar d_bound = bbox_parent.half_area() - bbox_in.half_area();
            while (true) {
                auto bbox_out = bvh->nodes[out].bbox;
                auto bbox_merged = BBox(bbox_in).extend(bbox_out);
                if (down) {
                    auto d_direct = bbox_parent.half_area() - bbox_merged.half_area();
                    if (d_best < d_direct + d) {
                        d_best = d_direct + d;
                        out_best = out;
                    }
                    d = d + bbox_out.half_area() - bbox_merged.half_area();
                    if (bvh->nodes[out].is_leaf || d_bound + d <= d_best)
                        down = false;
                    else
                        out = bvh->nodes[out].first_child_or_primitive;
                } else {
                    d = d - bbox_out.half_area() + bbox_merged.half_area();
                    if (pivot == parents[out]) {
                        bbox_pivot.extend(bbox_out);
                        out = pivot;
                        bbox_out = bvh->nodes[out].bbox;
                        if (out != parents[in]) {
                            bbox_merged = BBox(bbox_in).extend(bbox_pivot);
                            auto d_direct = bbox_parent.half_area() - bbox_merged.half_area();
                            if (d_best < d_direct + d) {
                                d_best = d_direct + d;
                                out_best = out;
                            }
                            d = d + bbox_out.half_area() - bbox_pivot.half_area();
                        }
                        if (out == 0)
                            break;
                        out = sibling(pivot);
                        pivot = parents[out];
                        down = true;
                    } else {
                        // If the node is the left sibling, go down
                        if (out % 2 == 1) {
                            down = true;
                            out = sibling(out);
                        } else {
                            out = parents[out];
                        }
                    }
                }
            }

            if (in == out_best || sibling(in) == out_best || parents[in] == out_best)
                return Insertion { 0, 0 };
            return Insertion { out_best, d_best };
        }
    };

    void optimize(size_t u = 9, Scalar threshold = 0.1) {
        using Insertion = typename Optimizer::Insertion;
        std::unique_ptr<std::atomic<int64_t>[]> locks(new std::atomic<int64_t>[node_count]);
        std::unique_ptr<Insertion[]> outs(new Insertion[node_count]);

        Optimizer optimizer(this);
        auto cost = optimizer.cost();
        for (size_t iteration = 0; ; ++iteration) {
            size_t first_node = iteration % u + 1;

            #pragma omp parallel
            {
                // Clear the locks
                #pragma omp for nowait
                for (size_t i = 0; i < node_count; i++)
                    locks[i] = 0;

                // Search for insertion candidates
                #pragma omp for
                for (size_t i = first_node; i < node_count; i += u)
                    outs[i] = optimizer.search(i);

                // Resolve topological conflicts with locking
                #pragma omp for
                for (size_t i = first_node; i < node_count; i += u) {
                    if (outs[i].second <= 0)
                        continue;
                    auto conflicts = optimizer.conflicts(i, outs[i].first);
                    // Encode locks into 64bits using the highest 32 bits for the cost and
                    // the lowest 32 bits for the index of the node requesting the re-insertion
                    auto lock = (int64_t(as<int32_t>(float(outs[i].second))) << 32) | (int64_t(i) & INT64_C(0xFFFFFFFF));
                    // This takes advantage of the fact that IEEE-754 floats can be compared with regular integer comparisons
                    for (auto c : conflicts)
                        atomic_max(locks[c], lock);
                }

                // Check the locks to disable conflicting re-insertions
                #pragma omp for
                for (size_t i = first_node; i < node_count; i += u) {
                    if (outs[i].second <= 0)
                        continue;
                    auto conflicts = optimizer.conflicts(i, outs[i].first);
                    // Make sure that this node owns all the locks for each and every conflicting node
                    if (!std::all_of(conflicts.begin(), conflicts.end(), [&] (size_t j) { return (locks[j] & INT64_C(0xFFFFFFFF)) == i; }))
                        outs[i] = Insertion { 0, 0 };
                }

                // Perform the reinsertions
                #pragma omp for
                for (size_t i = first_node; i < node_count; i += u) {
                    if (outs[i].second > 0)
                        optimizer.reinsert(i, outs[i].first);
                }

                // Refit the nodes that have changed
                #pragma omp for
                for (size_t i = first_node; i < node_count; i += u) {
                    if (outs[i].second > 0) {
                        optimizer.refit(i);
                        optimizer.refit(outs[i].first);
                    }
                }
            }

            auto new_cost = optimizer.cost();
            if (std::abs(new_cost - cost) <= threshold || iteration >= u) {
                if (u <= 1)
                    break;
                u = u - 1;
                iteration = 0;
            }
            cost = new_cost;
        }
    }

    template <typename T, size_t Size>
    struct TraversalStack {
        T elements[Size];
        size_t size = 0;

        void push(const T& t) { elements[size++] = t; }
        T pop() { return elements[--size]; }
        bool empty() const { return size == 0; }
    };

    /// Intersects the BVH with the given ray and intersector.
    template <bool AnyHit, typename Intersector>
    std::optional<std::pair<size_t, typename Intersector::Result>> intersect(Ray ray, const Intersector& intersector) const {
        auto best_hit = std::optional<std::pair<size_t, typename Intersector::Result>>(std::nullopt);

        auto intersect_leaf = [&] (const Node& node) {
            assert(node.is_leaf);
            auto primitive_count = node.primitive_count;
            auto first_primitive = node.first_child_or_primitive;
            for (size_t i = 0; i < primitive_count; ++i) {
                auto hit = intersector(first_primitive + i, ray);
                if (hit) {
                    best_hit = std::make_optional(std::make_pair(first_primitive + i, *hit));
                    if (AnyHit)
                        break;
                    ray.tmax = hit->distance;
                }
            }
            return best_hit;
        };

        // If the root is a leaf, intersect it and return
        if (nodes[0].is_leaf)
            return intersect_leaf(nodes[0]);

        // Precompute the inverse direction to avoid divisions and refactor
        // the computation to allow the use of FMA instructions (when available).
        auto inverse_direction = (glm::vec3(1.0f)/ray.direction);
        auto inverse_origin    = -ray.origin * inverse_direction;

        static constexpr size_t stack_size = max_depth + 3;

        // Indices into the node bounding box values are precomputed based on the ray octant
        int ix = ray.direction.x > Scalar(0) ? 0 : 3;
        int iy = ray.direction.y > Scalar(0) ? 1 : 4;
        int iz = ray.direction.z > Scalar(0) ? 2 : 5;

        // This traversal loop is eager, because it immediately processes leaves instead of pushing them on the stack.
        // This is generally beneficial for performance because intersections will likely be found which will
        // allow to cull more subtrees with the ray-box test of the traversal loop.
        TraversalStack<typename Node::Index, stack_size> stack;
        auto node = nodes.get();
        while (true) {
            auto first_child = node->first_child_or_primitive;

            auto& left  = nodes[first_child + 0];
            auto& right = nodes[first_child + 1];
            auto distance_left  = left .intersect(inverse_origin, inverse_direction, ray.tmin, ray.tmax, ix, iy, iz);
            auto distance_right = right.intersect(inverse_origin, inverse_direction, ray.tmin, ray.tmax, ix, iy, iz);
            bool hit_left  = distance_left.first  <= distance_left.second;
            bool hit_right = distance_right.first <= distance_right.second;

            if (hit_left && left.is_leaf) {
                if (intersect_leaf(left) && AnyHit)
                    break;
                hit_left = false;
            }

            if (hit_right && right.is_leaf) {
                if (intersect_leaf(right) && AnyHit)
                    break;
                hit_right = false;
            }

            if (hit_left && hit_right) {
                int order = distance_left.first < distance_right.first ? 0 : 1;
                stack.push(first_child + (1 - order));
                node = &nodes[first_child + order];
            } else if (hit_left ^ hit_right) {
                node = &nodes[first_child + (hit_left ? 0 : 1)];
            } else {
                if (stack.empty())
                    break;
                node = &nodes[stack.pop()];
            }
        }

        return best_hit;
    }

    std::unique_ptr<Node[]>   nodes;
    std::unique_ptr<size_t[]> primitive_indices;
    size_t                    node_count = 0;
    float                     traversal_cost = 1.5f;
};

}

#endif
