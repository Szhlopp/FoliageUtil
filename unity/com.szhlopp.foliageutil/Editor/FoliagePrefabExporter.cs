using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using UnityEditor;
using UnityEngine;

namespace FoliageUtil.Editor
{
    /// <summary>Persist a completed mesh, imported textures and materials without a runtime generator.</summary>
    public static class FoliagePrefabExporter
    {
        [Serializable] private sealed class MaterialPaths { public string base_color_texture, normal_texture, metallic_roughness_texture; }
        [Serializable] private sealed class MeshInfo { public string asset_base; public MaterialPaths[] materials; }
        public static string Save(FoliageGenerator generator, string assetFolder)
        {
            if (generator == null || generator.IsBuilding || string.IsNullOrEmpty(generator.LastMeshInfoJson)) throw new InvalidOperationException("Wait for a completed mesh before saving.");
            Mesh source = generator.GetComponent<MeshFilter>().sharedMesh;
            if (source == null || source.vertexCount == 0) throw new InvalidOperationException("The current growth state has no mesh.");
            string root = Path.GetFullPath(Application.dataPath), folder = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(Application.dataPath), assetFolder));
            if (!folder.StartsWith(root + Path.DirectorySeparatorChar, StringComparison.Ordinal)) throw new ArgumentException("Choose a new folder inside this project's Assets directory.");
            if (Directory.Exists(folder) || File.Exists(folder)) throw new IOException("Choose a new prefab folder; existing assets are preserved.");
            string relative = "Assets/" + folder.Substring(root.Length + 1).Replace('\\', '/');
            var info = JsonUtility.FromJson<MeshInfo>(generator.LastMeshInfoJson);
            Material[] originals = generator.GetComponent<MeshRenderer>().sharedMaterials;
            if (originals.Length != info.materials.Length) throw new InvalidOperationException("Current mesh material bindings have changed; regenerate before saving.");
            Directory.CreateDirectory(folder); AssetDatabase.Refresh();
            var mesh = UnityEngine.Object.Instantiate(source); mesh.name = "Foliage mesh"; mesh.hideFlags = HideFlags.None;
            AssetDatabase.CreateAsset(mesh, relative + "/mesh.asset");
            var materials = new Material[originals.Length]; var textures = new Dictionary<string, Texture2D>();
            for (int i = 0; i < materials.Length; ++i)
            {
                Material material = new Material(originals[i]) { hideFlags = HideFlags.None }; materials[i] = material;
                bool mask = material.GetFloat("_AlphaMode") == 1;
                float cutoff = material.GetFloat("_Cutoff");
                MaterialPaths paths = info.materials[i];
                ImportMap(material, "_BaseMap", paths.base_color_texture, false, mask, cutoff, info.asset_base, folder, relative, textures);
                ImportMap(material, "_NormalMap", paths.normal_texture, true, false, cutoff, info.asset_base, folder, relative, textures);
                ImportMap(material, "_PackedMap", paths.metallic_roughness_texture, true, false, cutoff, info.asset_base, folder, relative, textures);
                AssetDatabase.CreateAsset(material, relative + "/material-" + i + ".mat");
            }
            var gameObject = new GameObject(generator.gameObject.name);
            try
            {
                gameObject.AddComponent<MeshFilter>().sharedMesh = mesh;
                gameObject.AddComponent<MeshRenderer>().sharedMaterials = materials;
                string prefab = relative + "/foliage.prefab";
                if (PrefabUtility.SaveAsPrefabAsset(gameObject, prefab) == null) throw new IOException("Unity could not save the prefab.");
                AssetDatabase.SaveAssets(); return prefab;
            }
            finally { UnityEngine.Object.DestroyImmediate(gameObject); }
        }
        private static void ImportMap(Material material, string property, string file, bool linear, bool mask, float cutoff, string assetBase, string folder, string relative, Dictionary<string, Texture2D> textures)
        {
            if (string.IsNullOrEmpty(file)) return;
            string source = Path.GetFullPath(Path.Combine(assetBase, file));
            string role = (linear ? "data" : "color") + (mask ? cutoff.ToString("R", CultureInfo.InvariantCulture) : "opaque");
            string key = source + "|" + role;
            if (!textures.TryGetValue(key, out Texture2D texture))
            {
                byte[] bytes = File.ReadAllBytes(source);
                string hash;
                using (var sha = SHA256.Create()) hash = BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "").Substring(0, 24);
                using (var sha = SHA256.Create()) role = BitConverter.ToString(sha.ComputeHash(Encoding.UTF8.GetBytes(role))).Replace("-", "").Substring(0, 8);
                string name = hash + "-" + role + Path.GetExtension(source).ToLowerInvariant(), path = relative + "/" + name;
                File.WriteAllBytes(Path.Combine(folder, name), bytes);
                AssetDatabase.ImportAsset(path, ImportAssetOptions.ForceSynchronousImport);
                var importer = (TextureImporter)AssetImporter.GetAtPath(path);
                // The package shader reads OpenGL normal RGB directly, not Unity's packed normal encoding.
                importer.textureType = TextureImporterType.Default; importer.sRGBTexture = !linear;
                importer.alphaIsTransparency = !linear; importer.mipmapEnabled = true;
                importer.mipMapsPreserveCoverage = mask; importer.alphaTestReferenceValue = cutoff;
                importer.textureCompression = TextureImporterCompression.CompressedHQ;
                importer.maxTextureSize = 16384;
                importer.wrapMode = TextureWrapMode.Repeat; importer.filterMode = FilterMode.Trilinear; importer.anisoLevel = 4;
                importer.SaveAndReimport(); texture = AssetDatabase.LoadAssetAtPath<Texture2D>(path); textures.Add(key, texture);
            }
            material.SetTexture(property, texture);
        }
    }
}
