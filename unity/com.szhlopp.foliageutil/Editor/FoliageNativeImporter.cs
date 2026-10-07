using System.IO;
using UnityEditor;
using UnityEngine;

namespace FoliageUtil.Editor
{
    internal sealed class FoliageNativeImporter : AssetPostprocessor
    {
        private void OnPreprocessAsset() { if (assetImporter is PluginImporter plugin) Configure(plugin); }
        [InitializeOnLoadMethod] private static void Initialize() { EditorApplication.delayCall += ConfigureAll; }
        private static void ConfigureAll()
        {
            foreach (string guid in AssetDatabase.FindAssets("foliage_native"))
            {
                var plugin = AssetImporter.GetAtPath(AssetDatabase.GUIDToAssetPath(guid)) as PluginImporter;
                if (plugin != null && Configure(plugin)) plugin.SaveAndReimport();
            }
        }
        private static bool Configure(PluginImporter plugin)
        {
            string path = plugin.assetPath.Replace('\\', '/');
            string name = Path.GetFileNameWithoutExtension(path);
            if (name != "libfoliage_native" && name != "foliage_native") return false;
            string os = path.Contains("/Plugins/macOS/") ? "OSX" : path.Contains("/Plugins/Windows/") ? "Windows" : path.Contains("/Plugins/Linux/") ? "Linux" : null;
            if (os == null) return false;
            string cpu = path.Contains("/arm64/") ? "ARM64" : path.Contains("/universal/") ? "AnyCPU" : "x86_64";
            BuildTarget target = os == "OSX" ? BuildTarget.StandaloneOSX : os == "Windows" ? BuildTarget.StandaloneWindows64 : BuildTarget.StandaloneLinux64;
            bool changed = plugin.GetCompatibleWithAnyPlatform() || !plugin.GetCompatibleWithEditor() || plugin.GetEditorData("OS") != os || plugin.GetEditorData("CPU") != cpu;
            plugin.SetCompatibleWithAnyPlatform(false); plugin.SetCompatibleWithEditor(true);
            plugin.SetEditorData("OS", os); plugin.SetEditorData("CPU", cpu);
            foreach (BuildTarget other in new[] { BuildTarget.StandaloneOSX, BuildTarget.StandaloneWindows64, BuildTarget.StandaloneLinux64 })
            {
                bool enabled = other == target;
                changed |= plugin.GetCompatibleWithPlatform(other) != enabled;
                plugin.SetCompatibleWithPlatform(other, enabled);
            }
            changed |= plugin.GetPlatformData(target, "CPU") != cpu;
            plugin.SetPlatformData(target, "CPU", cpu);
            return changed;
        }
    }
    [CustomEditor(typeof(FoliageGenerator))]
    internal sealed class FoliageGeneratorEditor : UnityEditor.Editor
    {
        private string saveFolder = "Assets/FoliageUtilGenerated/Foliage01";
        public override void OnInspectorGUI()
        {
            DrawDefaultInspector();
            var generator = (FoliageGenerator)target;
            if (GUILayout.Button("Regenerate")) generator.RequestRebuild();
            if (GUILayout.Button("Reload recipe and textures")) generator.ReloadRecipe();
            EditorGUILayout.LabelField(generator.IsBuilding ? "Building foliage..." : "Ready", $"{generator.LastBuildMilliseconds:F1} ms");
            EditorGUILayout.LabelField("Main-thread apply", $"{generator.LastUploadMilliseconds:F1} ms");
            if (generator.ReuseMatureCardBake && !string.IsNullOrEmpty(generator.CardBakeProfile))
            {
                EditorGUILayout.LabelField("Last mature bake preparation", $"{generator.LastPreparationMilliseconds:F1} ms");
                EditorGUILayout.LabelField("Prepared bakes", generator.CardBakePreparationCount.ToString());
            }
            saveFolder = EditorGUILayout.TextField("New prefab folder", saveFolder);
            using (new EditorGUI.DisabledScope(generator.IsBuilding || string.IsNullOrEmpty(generator.LastMeshInfoJson)))
            {
                if (GUILayout.Button("Save current mesh as prefab"))
                {
                    try { string path = FoliagePrefabExporter.Save(generator, saveFolder); EditorGUIUtility.PingObject(AssetDatabase.LoadAssetAtPath<GameObject>(path)); }
                    catch (System.Exception error) { Debug.LogError("FoliageUtil prefab: " + error.Message, generator); }
                }
            }
            if (!string.IsNullOrEmpty(generator.LastError)) EditorGUILayout.HelpBox(generator.LastError, MessageType.Error);
            if (generator.IsBuilding) Repaint();
        }
    }
}
