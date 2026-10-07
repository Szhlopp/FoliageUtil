using System;
using System.IO;
using System.Linq;
using FoliageUtil;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

public static class AddBambooToDemo
{
    public static bool IsWorking { get; private set; }
    public static string Error { get; private set; }
    private static double started;
    private static FoliageGenerator bamboo;
    private static bool created;
    private const string ScenePath = "Assets/Scenes/Sakura.unity";

    [MenuItem("FoliageUtil/Add Bamboo Beside Sakura")]
    public static void Run()
    {
        if (IsWorking) return;
        try
        {
            Error = null;
            if (EditorApplication.isPlayingOrWillChangePlaymode) throw new InvalidOperationException("Leave Play mode before adding the bamboo demo.");
            if (!File.Exists(ScenePath) || !File.Exists("Assets/StreamingAssets/FoliageUtil/bamboo/graph.json")) throw new FileNotFoundException("Prepare the Sakura scene and portable bamboo bundle first.");
            if (!Application.isBatchMode && !EditorSceneManager.SaveCurrentModifiedScenesIfUserWantsTo()) return;
            if (EditorSceneManager.GetActiveScene().path != ScenePath) EditorSceneManager.OpenScene(ScenePath);
            var roots = EditorSceneManager.GetActiveScene().GetRootGameObjects();
            var sakura = roots.FirstOrDefault(item => item.name == "Sakura")?.GetComponent<FoliageGenerator>();
            if (sakura == null) throw new InvalidOperationException("The demo scene needs its Sakura generator.");
            bamboo = roots.FirstOrDefault(item => item.name == "Bamboo")?.GetComponent<FoliageGenerator>();
            created = bamboo == null;
            if (created)
            {
                var plant = new GameObject("Bamboo");
                Undo.RegisterCreatedObjectUndo(plant, "Add bamboo beside Sakura");
                plant.transform.position = sakura.transform.position + new Vector3(8, 0, 0);
                bamboo = plant.AddComponent<FoliageGenerator>();
                bamboo.RecipePath = "FoliageUtil/bamboo/graph.json";
                // This recipe also exports individual culms; select the complete grove explicitly.
                bamboo.SourceNode = "wind";
                bamboo.Growth = 1;
            }
            var controls = roots.SelectMany(item => item.GetComponentsInChildren<SakuraDemo>(true)).FirstOrDefault();
            if (controls == null) controls = new GameObject("Growth playback controls").AddComponent<SakuraDemo>();
            Undo.RecordObject(controls, "Connect bamboo playback");
            if (controls.Generator == null) controls.Generator = sakura;
            controls.Bamboo = bamboo;
            EditorUtility.SetDirty(controls);
            bamboo.RequestRebuild();
            started = EditorApplication.timeSinceStartup;
            IsWorking = true;
            EditorApplication.update += Finish;
        }
        catch (Exception error) { Error = error.Message; Debug.LogException(error); }
    }

    private static void Finish()
    {
        try
        {
            EditorApplication.QueuePlayerLoopUpdate();
            var plants = EditorSceneManager.GetActiveScene().GetRootGameObjects().SelectMany(item => item.GetComponentsInChildren<FoliageGenerator>()).ToArray();
            foreach (var plant in plants) if (plant.LastError != null) throw new Exception(plant.LastError);
            if (EditorApplication.timeSinceStartup - started > 180) throw new TimeoutException("Waiting for the foliage demo to generate");
            if (plants.Any(plant => plant.IsBuilding || plant.LastStatisticsJson == null)) return;
            if (created) FramePlants(plants);
            Selection.activeGameObject = bamboo.gameObject;
            EditorSceneManager.MarkSceneDirty(bamboo.gameObject.scene);
            if (!EditorSceneManager.SaveScene(bamboo.gameObject.scene, ScenePath)) throw new IOException("Could not save the updated demo scene");
            Debug.Log("Bamboo added beside Sakura. Play the scene and choose Both, Sakura or Bamboo in the growth controls.");
        }
        catch (Exception error) { Error = error.Message; Debug.LogException(error); }
        IsWorking = false;
        EditorApplication.update -= Finish;
    }

    private static void FramePlants(FoliageGenerator[] plants)
    {
        var camera = Camera.main;
        if (camera == null) return;
        var bounds = plants[0].GetComponent<MeshRenderer>().bounds;
        foreach (var plant in plants.Skip(1)) bounds.Encapsulate(plant.GetComponent<MeshRenderer>().bounds);
        var right = camera.transform.right;
        var up = camera.transform.up;
        float width = Vector3.Dot(new Vector3(Mathf.Abs(right.x), Mathf.Abs(right.y), Mathf.Abs(right.z)), bounds.extents);
        float height = Vector3.Dot(new Vector3(Mathf.Abs(up.x), Mathf.Abs(up.y), Mathf.Abs(up.z)), bounds.extents);
        Undo.RecordObject(camera, "Frame foliage demo");
        Undo.RecordObject(camera.transform, "Frame foliage demo");
        camera.orthographic = true;
        camera.orthographicSize = Mathf.Max(height, width / 1.6f) * 1.18f;
        camera.transform.position = bounds.center - camera.transform.forward * 40;
        if (SceneView.lastActiveSceneView != null) SceneView.lastActiveSceneView.LookAt(bounds.center, camera.transform.rotation, bounds.size.magnitude);
    }
}
