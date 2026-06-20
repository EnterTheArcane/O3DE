from __future__ import annotations

from pathlib import Path

from ..cps.cps import CPS, CPSComponentType


def _cmake_path(path: str) -> str:
    return path.replace("@prefix@", "${_TP_PREFIX}")


def _target(pkg_name: str, comp_name: str) -> str:
    return f"ThirdParty::{pkg_name}" if comp_name == pkg_name else f"ThirdParty::{comp_name}"


def _resolve_require(pkg_name: str, req: str, components: dict) -> str:
    if req.startswith(":"):
        return _target(pkg_name, req[1:])
    dep_pkg, dep_comp = req.split(":", 1)
    if dep_pkg == pkg_name:
        # Same-package require: conan-style keys include the package prefix, e.g.
        # require "OpenEXR:IexConfig" → key "OpenEXR::IexConfig" → ThirdParty::OpenEXR::IexConfig
        full_key = f"{dep_pkg}::{dep_comp}"
        if full_key in components:
            return _target(pkg_name, full_key)
        return _target(pkg_name, dep_comp)
    return _target(dep_pkg, dep_comp)


def generate(cps: CPS, pkg_dir: Path) -> None:
    pkg_name = cps.name
    lines: list[str] = []

    lines += [
        "get_filename_component(_TP_PREFIX \"${CMAKE_CURRENT_LIST_FILE}\" PATH)",
        "",
        f"if(TARGET ThirdParty::{pkg_name})",
        "    unset(_TP_PREFIX)",
        "    return()",
        "endif()",
        "",
    ]

    for comp_name, comp in cps.components.items():
        target = _target(pkg_name, comp_name)
        comp_type = CPSComponentType(comp.type)
        confs = cps.configurations or ["release"]

        if comp_type in (CPSComponentType.ARCHIVE, CPSComponentType.DYLIB, CPSComponentType.EXE):
            if comp_type == CPSComponentType.ARCHIVE:
                cmake_kind = "STATIC"
            elif comp_type == CPSComponentType.DYLIB:
                cmake_kind = "SHARED"
            else:
                cmake_kind = "EXECUTABLE"
            lines.append(f"add_library({target} {cmake_kind} IMPORTED GLOBAL)")
            if comp.location:
                for conf in confs:
                    lines.append(
                        f'set_target_properties({target} PROPERTIES'
                        f' IMPORTED_CONFIGURATIONS "{conf.upper()}"'
                        f' MAP_IMPORTED_CONFIG_DEBUG "{conf.upper()}"'
                        f' MAP_IMPORTED_CONFIG_RELWITHDEBINFO "{conf.upper()}"'
                        f' IMPORTED_LOCATION_{conf.upper()} "{_cmake_path(comp.location)}")'
                    )
            if comp.link_location:
                for conf in confs:
                    lines.append(
                        f'set_target_properties({target} PROPERTIES'
                        f' IMPORTED_IMPLIB_{conf.upper()} "{_cmake_path(comp.link_location)}")'
                    )
        else:
            lines.append(f"add_library({target} INTERFACE IMPORTED GLOBAL)")

        if comp.includes:
            paths = " ".join(f'"{_cmake_path(p)}"' for p in comp.includes)
            lines.append(f"target_include_directories({target} INTERFACE {paths})")

        if comp.definitions:
            for _lang, defs in comp.definitions.items():
                if defs:
                    defs_str = " ".join(
                        f'"{k}={v}"' if v is not None else f'"{k}"'
                        for k, v in defs.items()
                    )
                    lines.append(f"target_compile_definitions({target} INTERFACE {defs_str})")

        all_requires = list(comp.requires) + list(comp.link_requires)
        if all_requires:
            deps = " ".join(_resolve_require(pkg_name, r, cps.components) for r in all_requires)
            lines.append(f"target_link_libraries({target} INTERFACE {deps})")

        if comp.link_libraries:
            sys_libs = " ".join(comp.link_libraries)
            lines.append(f"target_link_libraries({target} INTERFACE {sys_libs})")

        lines.append("")

    lines += [
        f"set({pkg_name}_FOUND TRUE)",
        "unset(_TP_PREFIX)",
        "",
    ]

    pkg_dir.mkdir(parents=True, exist_ok=True)
    (pkg_dir / f"{pkg_name}-config.cmake").write_text("\n".join(lines), encoding="utf-8")

