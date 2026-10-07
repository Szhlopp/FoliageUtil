using System;
using System.IO;
using UnityEditor;
using UnityEditor.Build;
using UnityEditor.Build.Reporting;
using UnityEditor.SceneManagement;
using UnityEngine.Rendering;

public static class FoliagePlayerBuild
{
    public static void Run()
    {
        GraphicsSettings.defaultRenderPipeline=null;UnityEngine.QualitySettings.renderPipeline=null;
        EditorSceneManager.SaveScene(EditorSceneManager.NewScene(NewSceneSetup.EmptyScene),"Assets/PlayerSmoke.unity");
        bool mono=Array.IndexOf(Environment.GetCommandLineArgs(),"-foliageMono")>=0;
        PlayerSettings.SetScriptingBackend(NamedBuildTarget.Standalone,mono?ScriptingImplementation.Mono2x:ScriptingImplementation.IL2CPP);
        PlayerSettings.SetArchitecture(BuildTargetGroup.Standalone,1);
        string output=Path.GetFullPath(Path.Combine(UnityEngine.Application.dataPath,"../../unity-player/FoliageSmoke.app"));
        var report=BuildPipeline.BuildPlayer(new BuildPlayerOptions { scenes=new[]{"Assets/PlayerSmoke.unity"},locationPathName=output,target=BuildTarget.StandaloneOSX,options=BuildOptions.Development });
        if(report.summary.result!=BuildResult.Succeeded) throw new Exception("Player build failed: "+report.summary.result);
        EditorApplication.Exit(0);
    }
}
