#include "workspace.hpp"

namespace vox3d {

std::string_view ToString(WorkspaceTool tool)
{
    switch (tool) {
        case WorkspaceTool::kMap:
            return "map";
        case WorkspaceTool::kView:
            return "view";
        case WorkspaceTool::kLayers:
            return "layers";
        case WorkspaceTool::kObjects:
            return "objects";
        case WorkspaceTool::kRender:
            return "render";
        case WorkspaceTool::kDebug:
            return "debug";
        case WorkspaceTool::kSettings:
            return "settings";
    }
    return "unknown";
}

}  // namespace vox3d
