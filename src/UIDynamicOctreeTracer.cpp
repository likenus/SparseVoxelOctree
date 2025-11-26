//
// Created by Linus on 17.11.2025.
//

#include "UIDynamicOctreeTracer.hpp"


#include "ImGuiUtil.hpp"

void UI::DynamicOctreeTracerMenuItems(const std::shared_ptr<DynamicOctreeTracer> &octree_tracer) {
    if (ImGui::Button("Add Voxel") && !octree_tracer->GetOctreePtr()->IsRunning()) {
        octree_tracer->GetOctreePtr()->DoStep();
    }
    ImGui::Checkbox("Run", &octree_tracer->GetOctreePtr()->m_state);
}
