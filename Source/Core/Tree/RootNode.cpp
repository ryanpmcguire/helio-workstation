/*
    This file is part of Helio Workstation.

    Helio is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Helio is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Helio. If not, see <http://www.gnu.org/licenses/>.
*/

#include "Common.h"
#include "RootNode.h"

#include "TreeNodeSerializer.h"
#include "ProjectNode.h"
#include "ProjectTimeline.h"
#include "VersionControlNode.h"
#include "PatternEditorNode.h"
#include "TrackGroupNode.h"
#include "PianoTrackNode.h"

#include "Pattern.h"
#include "PianoSequence.h"
#include "ProjectMetadata.h"
#include "WorkspaceMenu.h"
#include "JsonSerializer.h"

#include "MainLayout.h"
#include "Workspace.h"
#include "Icons.h"

RootNode::RootNode(const String &name) :
    TreeNode(name, Serialization::Core::root) {}

String RootNode::getName() const noexcept
{
    return TRANS(I18n::Tree::root);
}

Image RootNode::getIcon() const noexcept
{
    return Icons::findByName(Icons::helio, Globals::UI::headlineIconSize);
}

void RootNode::showPage()
{
    if (this->dashboard == nullptr)
    {
        this->recreatePage();
    }

    App::Layout().showPage(this->dashboard.get(), this);
}

void RootNode::recreatePage()
{
    this->dashboard = make<Dashboard>(App::Layout());
}

//===----------------------------------------------------------------------===//
// Children
//===----------------------------------------------------------------------===//

ProjectNode *RootNode::openProject(const File &file)
{
    const auto myProjects(this->findChildrenOfType<ProjectNode>());

    // first check for duplicates (full path)
    for (auto *myProject : myProjects)
    {
        if (myProject->getDocument()->getFullPath() == file.getFullPathName())
        {
            myProject->selectChildOfType<PianoTrackNode, PatternEditorNode>();
            return nullptr;
        }
    }

    DBG("Opening project: " + file.getFullPathName());
    if (file.existsAsFile())
    {
        auto project = make<ProjectNode>(file);
        this->addChildNode(project.get(), 1);

        if (!project->getDocument()->load(file.getFullPathName()))
        {
            return nullptr;
        }

        // second check for duplicates (project id)
        for (auto *myProject : myProjects)
        {
            if (myProject->getId() == project->getId())
            {
                myProject->selectChildOfType<PianoTrackNode, PatternEditorNode>();
                return nullptr;
            }
        }

        project->selectChildOfType<PianoTrackNode, PatternEditorNode>();
        return project.release();
    }

    return nullptr;
}

ProjectNode *RootNode::checkoutProject(const String &id, const String &name)
{
    DBG("Cloning project: " + name);
    if (id.isNotEmpty())
    {
        // construct a stub project with no first revision and no tracks,
        // only the essential stuff it will need anyway:
        auto project = make<ProjectNode>(name, id);
        this->addChildNode(project.get(), 1);
        auto vcs = make<VersionControlNode>();
        project->addChildNode(vcs.get());
        project->addChildNode(new PatternEditorNode());
        vcs->cloneProject();
        vcs.release();
        return project.release();
    }

    return nullptr;
}

static ProjectNode *createProjectContentFromTemplate(ProjectNode *project, const String &templateName)
{
    int numBytes = 0;

    const auto resourceName = templateName.isNotEmpty() ? templateName : "emptyProject";
    const auto resourceNameFull = resourceName + "_json";
    auto *templatePtr = BinaryData::getNamedResource(resourceNameFull.getCharPointer(), numBytes);
    if (numBytes == 0)
    {
        // it might happen that templateName is already a full resource name:
        templatePtr = BinaryData::getNamedResource(resourceName.getCharPointer(), numBytes);
        jassert(numBytes > 0);
    }

    static JsonSerializer js;
    const auto exampleData = String(templatePtr, numBytes);
    const auto exampleProject = js.loadFromString(exampleData);

    // only load the content, i.e. tracks, the timeline, and the temperament:
    TreeNodeSerializer::deserializeChildren(*project, exampleProject);

    forEachChildWithType(exampleProject, node, Serialization::Core::projectTimeline)
    {
        project->getTimeline()->deserialize(node);
    }

    forEachChildWithType(exampleProject, node, Serialization::Core::projectInfo)
    {
        project->getProjectInfo()->getTemperament()->deserialize(node);
    }

    project->broadcastReloadProjectContent();
    const auto range = project->broadcastChangeProjectBeatRange();
    project->broadcastChangeViewBeatRange(range.getX(), range.getY());
    project->getDocument()->save();

    return project;
}

static void addAllEssentialProjectNodes(ProjectNode *parent)
{
    auto *vcs = new VersionControlNode();
    parent->addChildNode(vcs);

    // the vcs node must have a first commit
    // with all non-deletable items committed:
    vcs->commitProjectInfo();

    // also a must have:
    parent->addChildNode(new PatternEditorNode());
}

// this one is for desktops
ProjectNode *RootNode::addEmptyProject(const File &projectLocation, const String &templateName)
{
    auto *project = new ProjectNode(projectLocation);
    this->addChildNode(project);
    addAllEssentialProjectNodes(project);
    createProjectContentFromTemplate(project, templateName);
    project->selectChildOfType<PianoTrackNode>();
    return project;
}

// this one is for mobiles, where we don't have file chooser dialog
ProjectNode *RootNode::addEmptyProject(const String &projectName, const String &templateName)
{
    auto *project = new ProjectNode(projectName);
    this->addChildNode(project);
    addAllEssentialProjectNodes(project);
    createProjectContentFromTemplate(project, templateName);
    project->selectChildOfType<PianoTrackNode>();
    return project;
}

ProjectNode *RootNode::addExampleProject()
{
    auto *project = new ProjectNode(TRANS(I18n::Defaults::newProjectName));
    this->addChildNode(project);
    addAllEssentialProjectNodes(project);
    createProjectContentFromTemplate(project, "exampleProject_json");
    project->selectChildOfType<PianoTrackNode>();
    return project;
}

ProjectNode *RootNode::importMidi(const File &file)
{
    auto *project = new ProjectNode(file.getFileNameWithoutExtension());
    this->addChildNode(project);
    addAllEssentialProjectNodes(project);
    project->importMidi(file);
    project->selectChildOfType<PianoTrackNode, PatternEditorNode>();
    return project;
}

//===----------------------------------------------------------------------===//
// Menu
//===----------------------------------------------------------------------===//

bool RootNode::hasMenu() const noexcept
{
    return true;
}

UniquePointer<Component> RootNode::createMenu()
{
    return make<WorkspaceMenu>(App::Workspace());
}

//===----------------------------------------------------------------------===//
// Serializable
//===----------------------------------------------------------------------===//

void RootNode::deserialize(const SerializedData &data)
{
    const auto root = data.hasType(Serialization::Core::treeNode) ?
        data : data.getChildWithName(Serialization::Core::treeNode);

    if (root.isValid())
    {
        TreeNode::deserialize(root);
    }
}
