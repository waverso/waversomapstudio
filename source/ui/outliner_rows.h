// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <algorithm>
#include <vector>

#include "../scene/scene.h"

class OutlinerRows
{
    public:
    struct Row
    {
        int index;
        int depth;
        bool selfMatch;
    };

    struct Drop
    {
        int target = -1;
        SceneDropMode mode = SceneDropRoot;
        int depth = 0;
    };

    Drop DropAt(const EditorScene& sc, int boundary, int depth) const
    {
        Drop drop;
        if (boundary == static_cast<int>(rows.size()) && depth <= 0)
        {
            return drop;
        }
        const Row* before = nullptr;
        const Row* after = nullptr;
        int selectedDepth = -1;
        for (int index = 0; index < static_cast<int>(rows.size()); ++index)
        {
            const auto& row = rows[index];
            if (selectedDepth >= 0 && row.depth > selectedDepth)
            {
                continue;
            }
            selectedDepth = sc.items[row.index].selected ? row.depth : -1;
            if (selectedDepth < 0)
            {
                if (index < boundary)
                {
                    before = &row;
                }
                else
                {
                    after = &row;
                    break;
                }
            }
        }
        const int minimum = after ? after->depth : 0;
        int maximum = minimum;
        if (before)
        {
            const int edgeDepth
                = (std::max)(rows[boundary - 1].depth, boundary < static_cast<int>(rows.size()) ? rows[boundary].depth : 0);
            maximum = (std::max)(minimum, (std::min)(edgeDepth, before->depth + 1));
        }
        drop.depth = std::clamp(depth, minimum, maximum);
        if (after && drop.depth == after->depth)
        {
            drop.target = after->index;
            drop.mode = SceneDropBefore;
        }
        else if (before && drop.depth > 0)
        {
            drop.target = before->index;
            for (int level = before->depth; level >= drop.depth; --level)
            {
                drop.target = sc.items[drop.target].parent;
            }
            drop.mode = SceneDropInto;
        }
        return drop;
    }

    template <class Match>
    void Build(const EditorScene& sc, bool filtering, Match match)
    {
        rows.clear();
        rows.reserve(sc.count);
        flags.resize(sc.count);
        const auto& order = sc.DisplayOrder();
        for (int index : order)
        {
            flags[index] = match(sc.items[index]) ? 3 : 0;
        }
        if (filtering)
        {
            for (auto it = order.rbegin(); it != order.rend(); ++it)
            {
                const int parent = sc.items[*it].parent;
                if (parent >= 0 && (flags[*it] & 2))
                {
                    flags[parent] |= 2;
                }
            }
        }
        for (int index : order)
        {
            const int parent = sc.items[index].parent;
            const bool visible = (!filtering || (flags[index] & 2))
                && (parent < 0 || ((flags[parent] & 4) && (filtering || sc.items[parent].open)));
            if (visible)
            {
                flags[index] |= 4;
                rows.push_back({ index, sc.HierarchyDepth(index), (flags[index] & 1) != 0 });
            }
        }
    }

    void SelectAll(EditorScene& sc) const
    {
        for (const auto& row : rows)
        {
            sc.items[row.index].selected = true;
        }
    }

    bool SelectRange(EditorScene& sc, int anchorId, int targetId) const
    {
        int anchor = -1;
        int target = -1;
        for (int i = 0; i < static_cast<int>(rows.size()); ++i)
        {
            const int id = sc.items[rows[i].index].id;
            if (id == anchorId)
            {
                anchor = i;
            }
            if (id == targetId)
            {
                target = i;
            }
        }
        if (anchor < 0 || target < 0)
        {
            return false;
        }
        for (int i = (std::min)(anchor, target); i <= (std::max)(anchor, target); ++i)
        {
            sc.items[rows[i].index].selected = true;
        }
        return true;
    }

    std::vector<Row> rows;

    private:
    std::vector<unsigned char> flags;
};
