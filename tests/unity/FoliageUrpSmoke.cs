using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

public static class FoliageUrpSmoke
{
    public static void Run()
    {
        var data=ScriptableObject.CreateInstance<UniversalRendererData>();
        var asset=UniversalRenderPipelineAsset.Create(data);
        GraphicsSettings.defaultRenderPipeline=asset;QualitySettings.renderPipeline=asset;
        if (System.Array.IndexOf(System.Environment.GetCommandLineArgs(), "-foliageReuseBake") >= 0) FoliageCardGrowthSmoke.Run();
        else if (System.Array.IndexOf(System.Environment.GetCommandLineArgs(), "-foliageMinerals") >= 0) FoliageMineralSmoke.Run();
        else FoliageSmoke.Run();
    }
}
