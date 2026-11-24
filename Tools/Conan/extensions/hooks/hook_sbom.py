#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

import json
from pathlib import Path
from urllib.parse import quote
from uuid import NAMESPACE_URL, uuid5

from conan.tools.files import save
from conan.tools.sbom import cyclonedx_1_6
from conan.tools.sbom.spdx_licenses import VALID_SPDX_LICENSES

SBOM_FILENAME = "sbom.cdx.json"
SPDX_IDS = {license_id.lower(): license_id for license_id in VALID_SPDX_LICENSES}


def format_sbom(bom):
    metadata = bom.get("metadata", {})
    metadata.pop("tools", None)
    components = list(bom.get("components", ()))
    if "component" in metadata:
        components.append(metadata["component"])

    references = {}
    for component in components:
        if not component.get("purl", "").startswith("pkg:conan/"):
            continue
        name = quote(component["name"], safe="")
        version = quote(component["version"], safe="")
        component["purl"] = f"pkg:generic/{name}@{version}"
        ref = component["bom-ref"]
        component["bom-ref"] = references[ref] = uuid5(NAMESPACE_URL, ref).urn

    for edge in bom.get("dependencies", ()):
        edge["ref"] = references.get(edge["ref"], edge["ref"])
        if "dependsOn" in edge:
            edge["dependsOn"] = [references.get(ref, ref) for ref in edge["dependsOn"]]

    return json.dumps(bom, indent=2, sort_keys=True) + "\n"


def package_sbom(conanfile):
    bom = cyclonedx_1_6(conanfile, add_build=conanfile.context == "build")
    bom["$schema"] = "http://cyclonedx.org/schema/bom-1.6.schema.json"

    # The SBOM is part of the package revision and needs reproducible bytes.
    bom.pop("serialNumber", None)
    metadata = bom["metadata"]
    metadata.pop("timestamp", None)

    components = {component["bom-ref"]: component for component in bom["components"]}
    for component in components.values():
        for choice in component.get("licenses", ()):
            license = choice.get("license", choice)
            if "id" in license:
                license["id"] = SPDX_IDS[license["id"].lower()]
            license["acknowledgement"] = "declared"

    # Conan reports the root in both places. A bom-ref can occur only once.
    metadata["component"] = components.pop(metadata["component"]["bom-ref"])
    bom["components"] = [components[ref] for ref in sorted(components)]

    dependencies = {}
    for edge in bom.get("dependencies", ()):
        dependencies.setdefault(edge["ref"], set()).update(edge.get("dependsOn", ()))
    bom["dependencies"] = [
        {"ref": ref, "dependsOn": sorted(dependencies[ref] - {ref})}
        for ref in sorted(dependencies)
    ]
    return format_sbom(bom)


def post_package(conanfile):
    path = Path(conanfile.package_folder) / SBOM_FILENAME
    if not path.is_file():
        save(conanfile, path, package_sbom(conanfile))
