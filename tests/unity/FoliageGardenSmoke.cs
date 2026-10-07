using System;
using System.IO;
using System.Linq;
using FoliageUtil;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Rendering;

// Runs only when explicitly invoked; never starts or exits during interactive use.
public static class FoliageGardenSmoke
{
    private static double started;
    private static int step, phase;
    private static FoliageGenerator bamboo, sakura;
    private static float sakuraGrowth;
    private static Vector3 sakuraPosition;
    private static readonly float[] Progress = { 0, .4f, .6f, .8f, 1 };
    [Serializable] private sealed class Stats { public double progress; }
    public static void Run()
    {
        AddBambooToDemo.Run();
        started = EditorApplication.timeSinceStartup;
        EditorApplication.update += Tick;
    }
    private static void Tick()
    {
        try
        {
            EditorApplication.QueuePlayerLoopUpdate();
            if (AddBambooToDemo.Error != null) throw new Exception(AddBambooToDemo.Error);
            if (EditorApplication.timeSinceStartup - started > 240) throw new TimeoutException("Garden smoke timed out");
            if (AddBambooToDemo.IsWorking) return;
            if (bamboo == null)
            {
                bamboo = GameObject.Find("Bamboo").GetComponent<FoliageGenerator>();
                sakura = GameObject.Find("Sakura").GetComponent<FoliageGenerator>();
                sakuraGrowth = sakura.Growth; sakuraPosition = sakura.transform.position;
                Render("garden-mature.png", Camera.main);
                string scene = File.ReadAllText("Assets/Scenes/Sakura.unity");
                if (scene.Contains("\nMesh:") || scene.Contains("\nTexture2D:") || scene.Contains("\nMaterial:") || scene.Length > 100000) throw new Exception("The scene embedded generated assets");
                // Reopening verifies the saved references and on-enable regeneration.
                EditorSceneManager.OpenScene("Assets/Scenes/Sakura.unity");
                bamboo = GameObject.Find("Bamboo").GetComponent<FoliageGenerator>();
                sakura = GameObject.Find("Sakura").GetComponent<FoliageGenerator>();
                if (sakura.Growth != sakuraGrowth || sakura.transform.position != sakuraPosition) throw new Exception("Existing Sakura settings changed on reload");
                var controls = GameObject.Find("Growth playback controls").GetComponent<SakuraDemo>();
                if (controls.Bamboo != bamboo || controls.Generator != sakura) throw new Exception("Demo growth controls lost references");
                bamboo.SetGrowth(Progress[0]); return;
            }
            if (bamboo.LastError != null) throw new Exception(bamboo.LastError);
            if (sakura.LastError != null) throw new Exception(sakura.LastError);
            if (phase != 0)
            {
                float expected = phase == 1 ? 0 : phase == 2 ? .6f : 1;
                if (!AtProgress(bamboo, expected) || !AtProgress(sakura, phase == 3 ? sakuraGrowth : expected)) return;
                var controls = GameObject.Find("Growth playback controls").GetComponent<SakuraDemo>();
                if (phase == 1)
                {
                    if (bamboo.GetComponent<MeshFilter>().sharedMesh.vertexCount != 0 || sakura.GetComponent<MeshFilter>().sharedMesh.vertexCount != 0) throw new Exception("Both playback did not clear both plants at birth");
                    controls.SetProgress(.6f); phase = 2; return;
                }
                if (phase == 2)
                {
                    if (bamboo.GetComponent<MeshFilter>().sharedMesh.vertexCount == 0 || sakura.GetComponent<MeshFilter>().sharedMesh.vertexCount == 0) throw new Exception("Both playback did not regrow both plants");
                    Render("garden-growing.png", Camera.main);
                    sakura.SetGrowth(sakuraGrowth); bamboo.SetGrowth(1); phase = 3; return;
                }
                EditorSceneManager.SaveScene(bamboo.gameObject.scene);
                Debug.Log("FOLIAGE_GARDEN_PASSED: saved/reopened scene, empty/partial/mature bamboo, shared playback, preserved Sakura settings, lightweight serialization.");
                EditorApplication.update -= Tick; EditorApplication.Exit(0); return;
            }
            if (bamboo.IsBuilding || bamboo.LastStatisticsJson == null) return;
            if (Math.Abs(JsonUtility.FromJson<Stats>(bamboo.LastStatisticsJson).progress - Progress[step]) > .00001) return;
            var mesh = bamboo.GetComponent<MeshFilter>().sharedMesh;
            if (step == 0 && mesh.vertexCount != 0) throw new Exception("Bamboo did not start empty");
            if (step > 0 && mesh.vertexCount == 0) throw new Exception("Bamboo stage is unexpectedly empty");
            if (step > 0)
            {
                var closeup = new GameObject("Bamboo verification camera").AddComponent<Camera>();
                closeup.CopyFrom(Camera.main);
                Vector3 center = bamboo.transform.position + new Vector3(0, 2.5f, 0);
                closeup.transform.position = center + new Vector3(3, 1, -12);
                closeup.transform.LookAt(center); closeup.orthographicSize = 3.05f;
                Render("bamboo-" + step + ".png", closeup);
                UnityEngine.Object.DestroyImmediate(closeup.gameObject);
            }
            if (++step < Progress.Length) { bamboo.SetGrowth(Progress[step]); return; }
            if (mesh.vertexCount != 365673) throw new Exception("Bamboo mature geometry changed");
            GameObject.Find("Growth playback controls").GetComponent<SakuraDemo>().SetProgress(0); phase = 1;
        }
        catch (Exception error) { Debug.LogException(error); EditorApplication.update -= Tick; EditorApplication.Exit(1); }
    }
    private static bool AtProgress(FoliageGenerator plant, float expected) { return !plant.IsBuilding && plant.LastStatisticsJson != null && Math.Abs(JsonUtility.FromJson<Stats>(plant.LastStatisticsJson).progress - expected) < .00001; }
    private static void Render(string filename, Camera camera)
    {
        var target = new RenderTexture(1600, 1000, 24);
        camera.targetTexture = target;
        if (GraphicsSettings.defaultRenderPipeline == null) camera.Render();
        else RenderPipeline.SubmitRenderRequest(camera, new RenderPipeline.StandardRequest { destination = target });
        RenderTexture.active = target;
        var pixels = new Texture2D(1600, 1000, TextureFormat.RGB24, false);
        pixels.ReadPixels(new Rect(0, 0, 1600, 1000), 0, 0); pixels.Apply();
        string output = Path.GetFullPath(Path.Combine(Application.dataPath, "../Previews")); Directory.CreateDirectory(output);
        File.WriteAllBytes(Path.Combine(output, filename), pixels.EncodeToPNG());
        camera.targetTexture = null; RenderTexture.active = null;
        UnityEngine.Object.DestroyImmediate(pixels); UnityEngine.Object.DestroyImmediate(target);
    }
}
