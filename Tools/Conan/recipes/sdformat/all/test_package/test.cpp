/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

#include <sdf/InstallationDirectories.hh>
#include <sdf/Model.hh>
#include <sdf/Root.hh>

int main()
{
#ifndef __EMSCRIPTEN__
    const auto schema = std::filesystem::path(sdf::getSharePath()) / "sdformat/1.12/root.sdf";
    if (!std::filesystem::is_regular_file(schema))
    {
        std::cerr << "Missing installed schema " << schema << '\n';
        return 1;
    }
#endif

    constexpr std::string_view models[] = {
        R"(<sdf version="1.12"><model name="test"><link name="base"/></model></sdf>)",
        R"(<robot name="test">
              <link name="base">
                <inertial>
                  <mass value="1"/>
                  <inertia ixx="1" ixy="0" ixz="0" iyy="1" iyz="0" izz="1"/>
                </inertial>
              </link>
            </robot>)"
    };

    for (const auto model : models)
    {
        sdf::Root root;
        const auto errors = root.LoadSdfString(std::string(model));
        for (const auto& error : errors)
        {
            std::cerr << error.Message() << '\n';
        }
        if (!errors.empty() || !root.Model() || root.Model()->Name() != "test" ||
            !root.Model()->LinkByName("base"))
        {
            return 1;
        }
    }
    return 0;
}
