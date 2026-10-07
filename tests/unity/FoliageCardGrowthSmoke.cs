using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Diagnostics;
using FoliageUtil;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

// Real editor test: exact mature comparison, shared GPU textures, empty/backward
// growth, settings invalidation and measured worker/apply timings with real maps.
public static class FoliageCardGrowthSmoke
{
    [Serializable] class Stats { public double progress; }
    [Serializable] class Sample { public float progress; public int vertices, triangles; public double worker_ms, upload_ms; }
    [Serializable] class Result { public bool verified; public double reference_bake_ms, preparation_ms, initial_upload_ms, initial_load_ms; public int preparation_count; public List<Sample> samples = new List<Sample>(); }
    static FoliageGenerator generator;
    static MeshSnapshot reference;
    static Result result = new Result();
    static List<float> progress = new List<float> { 0, .15f, .35f, .55f, .75f, 1, .55f, 0 };
    static int step;
    static string sample, profile, output, directory;
    static double started;
    static Material[] materials;
    static Texture[] textures;
    static Dictionary<string, DateTime> timestamps;
    static Bounds bounds;
    static bool invalidating, reloading, disabling;
    static Material[] reloadMaterials;

    public static void Run()
    {
        try
        {
            var args = Environment.GetCommandLineArgs();
            sample = args[Array.IndexOf(args, "-foliageSample") + 1]; profile = args[Array.IndexOf(args, "-foliageBake") + 1];
            output = Path.GetFullPath(Path.Combine(Application.dataPath, "../Artifacts")); Directory.CreateDirectory(output);
            PlayerSettings.colorSpace = ColorSpace.Linear; FoliageSmoke.CheckCulling();
            string path = Path.Combine(Application.streamingAssetsPath, "FoliageUtil/" + sample + "/graph.json");
            using (var recipe = new FoliageRecipe(File.ReadAllText(path), Path.GetDirectoryName(path)))
            {
                var clock = Stopwatch.StartNew(); reference = recipe.BakeCards(profile, Path.Combine(output, "reference"), 1, 42); result.reference_bake_ms = clock.Elapsed.TotalMilliseconds;
            }
            for (int i = 0; i < reference.VertexCount; ++i)
            {
                int n = i * 14; var point = new Vector3(reference.Vertices[n], reference.Vertices[n + 1], reference.Vertices[n + 2]);
                if (i == 0) bounds = new Bounds(point, Vector3.zero); else bounds.Encapsulate(point);
            }
            for (int i = 0; i <= 24; ++i) progress.Add(.25f + .75f * i / 24);
            generator = new GameObject("Reusable mature cards").AddComponent<FoliageGenerator>();
            generator.CardBakeProfile = profile; generator.ReuseMatureCardBake = true; generator.Growth = progress[0];
            generator.RecipePath = "FoliageUtil/" + sample + "/graph.json"; generator.RequestRebuild();
            started = EditorApplication.timeSinceStartup; EditorApplication.update += Tick;
        }
        catch (Exception error) { Fail(error); }
    }
    static void Require(bool value, string message) { if (!value) throw new Exception(message); }
    static void Tick()
    {
        try
        {
            Require(EditorApplication.timeSinceStartup - started < 240, "Timed out waiting for cached growth");
            EditorApplication.QueuePlayerLoopUpdate();
            if (disabling)
            {
                if (Directory.Exists(directory)) return;
                result.verified = true; File.WriteAllText(Path.Combine(output, "card-growth-smoke.json"), JsonUtility.ToJson(result, true));
                UnityEngine.Debug.Log("FOLIAGE_UNITY_SMOKE_PASSED"); EditorApplication.update -= Tick; EditorApplication.Exit(0); return;
            }
            Require(generator.LastError == null, generator.LastError);
            if (reloading && generator.IsBuilding) Require(generator.GetComponent<MeshRenderer>().sharedMaterials.SequenceEqual(reloadMaterials), "Reload replaced visible materials before the new bake was ready");
            if (generator.IsBuilding || generator.LastStatisticsJson == null) return;
            float current = (float)JsonUtility.FromJson<Stats>(generator.LastStatisticsJson).progress;
            if (reloading)
            {
                if (generator.CardBakePreparationCount != 3) return;
                Require(generator.LastBakeDirectory != directory, "Recipe reload must refresh the mature bake");
                if (Directory.Exists(directory)) return;
                directory = generator.LastBakeDirectory; result.preparation_count = generator.CardBakePreparationCount;
                generator.enabled = false; disabling = true; return;
            }
            if (invalidating)
            {
                if (generator.CardBakePreparationCount != 2) return;
                Require(generator.LastBakeDirectory != directory, "Seed changes must prepare a new binding/atlas package");
                if (Directory.Exists(directory)) return;
                directory = generator.LastBakeDirectory; reloadMaterials = generator.GetComponent<MeshRenderer>().sharedMaterials;
                generator.ReloadRecipe(); reloading = true; return;
            }
            if (Math.Abs(current - progress[step]) > 1e-6) return;
            var mesh = generator.GetComponent<MeshFilter>().sharedMesh;
            var renderer = generator.GetComponent<MeshRenderer>();
            Require(generator.CardBakePreparationCount == 1, "Growth changes must reuse one mature bake");
            if (step == 0)
            {
                directory = generator.LastBakeDirectory; materials = renderer.sharedMaterials;
                textures = materials.SelectMany(m => new[] { m.GetTexture("_BaseMap"), m.GetTexture("_NormalMap"), m.GetTexture("_PackedMap") }).ToArray();
                timestamps = Directory.GetFiles(directory, "*", SearchOption.AllDirectories).ToDictionary(p => p, File.GetLastWriteTimeUtc);
                result.preparation_ms = generator.LastPreparationMilliseconds; result.initial_load_ms = generator.LastLoadMilliseconds; result.initial_upload_ms = generator.LastUploadMilliseconds;
            }
            Require(directory == generator.LastBakeDirectory, "Texture directory changed during growth");
            Require(renderer.sharedMaterials.SequenceEqual(materials), "Materials were reloaded during growth");
            Require(materials.SelectMany(m => new[] { m.GetTexture("_BaseMap"), m.GetTexture("_NormalMap"), m.GetTexture("_PackedMap") }).SequenceEqual(textures), "GPU textures were recreated during growth");
            foreach (var file in timestamps) Require(File.GetLastWriteTimeUtc(file.Key) == file.Value, "Cached growth rewrote the package");
            Require(!generator.LastStatisticsJson.Contains("\"node\":\"flowers\"") && !generator.LastStatisticsJson.Contains("\"node\":\"leaves\""), "Detailed canopy executed during cached growth");
            if (current == 0) Require(mesh.vertexCount == 0 && !renderer.enabled, "Backward or initial empty growth retained geometry");
            else Require(mesh.vertexCount > 0 && renderer.enabled, "Nonempty growth invisible");
            if (current == 1) { CompareMature(mesh); File.WriteAllText(Path.Combine(output, "native-growth.json"), generator.LastStatisticsJson); }
            result.samples.Add(new Sample { progress = current, vertices = mesh.vertexCount, worker_ms = generator.LastBuildMilliseconds, upload_ms = generator.LastUploadMilliseconds });
            result.samples[result.samples.Count - 1].triangles = Enumerable.Range(0, mesh.subMeshCount).Sum(i => (int)(mesh.GetIndexCount(i) / 3));
            if (step >= 2 && step <= 5) { Render(current, false); Render(current, true); }
            ++step;
            if (step < progress.Count) { generator.SetGrowth(progress[step]); return; }
            Require(materials.All(m => m.hideFlags == HideFlags.DontSave), "Live materials must stay temporary");
            Require(mesh.hideFlags == HideFlags.DontSave, "Live mesh must stay temporary");
            string scene = "Assets/CardGrowthSaveTest.unity";
            Require(EditorSceneManager.SaveScene(generator.gameObject.scene, scene), "Could not save scene");
            string saved = File.ReadAllText(scene);
            Require(!saved.Contains("\nMesh:") && !saved.Contains("\nTexture2D:") && !saved.Contains("\nMaterial:") && new FileInfo(scene).Length < 100000, "Generated buffers leaked into saved scene");
            generator.Seed = 43; generator.RequestRebuild(); invalidating = true;
        }
        catch (Exception error) { Fail(error); }
    }
    static void CompareMature(Mesh mesh)
    {
        Require(mesh.vertexCount == reference.VertexCount, "Mature cached topology differs from a direct bake");
        var positions = mesh.vertices; var normals = mesh.normals; var uv = mesh.uv;
        for (int i = 0; i < mesh.vertexCount; ++i)
        {
            int n = i * 14;
            Require(Vector3.Distance(positions[i], new Vector3(reference.Vertices[n], reference.Vertices[n + 1], reference.Vertices[n + 2])) < 1e-6f, "Mature cached position mismatch");
            Require(Vector3.Distance(normals[i], new Vector3(reference.Vertices[n + 3], reference.Vertices[n + 4], reference.Vertices[n + 5])) < 1e-6f, "Mature cached normal mismatch");
            Require(Math.Abs(uv[i].x - reference.Vertices[n + 10]) < 1e-6f && Math.Abs(uv[i].y - reference.Vertices[n + 11]) < 1e-6f, "Mature atlas UV mismatch");
        }
        Require(mesh.subMeshCount == reference.Submeshes.Length, "Material slot mismatch");
        for (int slot = 0; slot < mesh.subMeshCount; ++slot) Require(mesh.GetIndices(slot).SequenceEqual(reference.Indices.Skip((int)reference.Submeshes[slot].IndexStart).Take((int)reference.Submeshes[slot].IndexCount)), "Mature index mismatch");
        Require(mesh.tangents.Length == mesh.vertexCount && mesh.tangents.All(t => !float.IsNaN(t.x) && Math.Abs(t.w) == 1), "Invalid cached tangents");
    }
    static void Render(float growth, bool side)
    {
        RenderSettings.ambientMode = UnityEngine.Rendering.AmbientMode.Flat; RenderSettings.ambientLight = new Color(.35f, .38f, .42f);
        var light = new GameObject("Test sun").AddComponent<Light>(); light.type = LightType.Directional; light.intensity = 2.2f; light.transform.rotation = Quaternion.Euler(40, -30, 0);
        var camera = new GameObject("Test camera").AddComponent<Camera>(); camera.transform.position = bounds.center + (side ? new Vector3(-3, .25f, 1) : new Vector3(1, .25f, -3)).normalized * bounds.size.magnitude * 2;
        camera.transform.LookAt(bounds.center); camera.orthographic = true; camera.orthographicSize = Mathf.Max(bounds.extents.y, bounds.extents.x) * 1.18f; camera.farClipPlane = 1000; camera.clearFlags = CameraClearFlags.SolidColor; camera.backgroundColor = new Color(.08f, .1f, .12f);
        var target = new RenderTexture(768, 768, 24); camera.targetTexture = target;
        if (UnityEngine.Rendering.GraphicsSettings.defaultRenderPipeline == null) camera.Render();
        else UnityEngine.Rendering.RenderPipeline.SubmitRenderRequest(camera, new UnityEngine.Rendering.RenderPipeline.StandardRequest { destination = target });
        RenderTexture.active = target; var image = new Texture2D(768, 768, TextureFormat.RGB24, false); image.ReadPixels(new Rect(0, 0, 768, 768), 0, 0); image.Apply();
        File.WriteAllBytes(Path.Combine(output, $"growth-{growth:F2}-{(side ? "side" : "front")}.png"), image.EncodeToPNG());
        RenderTexture.active = null; camera.targetTexture = null;
        UnityEngine.Object.DestroyImmediate(image); UnityEngine.Object.DestroyImmediate(target); UnityEngine.Object.DestroyImmediate(camera.gameObject); UnityEngine.Object.DestroyImmediate(light.gameObject);
    }
    static void Fail(Exception error) { UnityEngine.Debug.LogException(error); EditorApplication.update -= Tick; EditorApplication.Exit(1); }
}
