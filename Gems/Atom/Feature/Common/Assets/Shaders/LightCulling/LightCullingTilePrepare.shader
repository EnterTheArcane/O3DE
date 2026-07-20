{
    "Source": "LightCullingTilePrepare.slang",
    
    "ProgramSettings" : 
    {
        "EntryPoints":
        [
            {
                "name":  "MainCS",
                "type" : "Compute"
            }
        ] 
    },

    "Supervariants":
    [
        {
            "Name": "NoMSAA",
            "AddBuildArguments": {
                "azslc": ["--no-ms"]
            }
        }
    ]
}
