#include "tool_view.h"
#include "tool_panel.h"
RenderedView renderToolView(const ViewSpec &spec) {
    ToolDefinition preview;
    preview.limits = {{"max_files", 1000}, {"max_input_bytes", 5242880}};
    auto *panel = new ToolPanel(preview, spec);
    return {panel, panel->editor()};
}
