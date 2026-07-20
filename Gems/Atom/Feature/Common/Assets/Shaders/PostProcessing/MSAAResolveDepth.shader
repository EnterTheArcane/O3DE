{ 
    "Source" : "MSAAResolveDepth.slang",

    "DepthStencilState" : { 
        "Depth" : { "Enable" : true, "CompareFunc" : "Always" }
    }, 

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
