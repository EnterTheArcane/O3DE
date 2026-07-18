{
    "Source": "SlangShaderOptions.slang",

    "DisabledRHIBackends": ["null"],

    "DrawList": "forward",

    // The default supervariant specializes its options (engine-default -sc-options); this one
    // opts out, so its variants bake pinned options at link time and leave the rest reading the
    // ShaderVariantKey fallback — the same split AZSL shaders get without specialization.
    "Supervariants": [
        {
            "Name": "NoSpecialization",
            "RemoveBuildArguments": {
                "namedArgumentGroups": { "slang": ["-sc-options"] }
            }
        }
    ],

    "ProgramSettings":
    {
        "EntryPoints":
        [
            {
                "name": "MainVS",
                "type": "Vertex"
            },
            {
                "name": "MainPS",
                "type": "Fragment"
            }
        ]
    }
}
