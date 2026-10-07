using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

namespace FoliageUtil
{
    [Serializable] internal sealed class MaterialRecipe
    {
        public string name, base_color_texture, normal_texture, metallic_roughness_texture, alpha_mode;
        public float[] base_color, translucency_color;
        public float roughness = 1, metallic, alpha_cutoff = .5f, translucency;
        public bool double_sided = true;
    }
    [Serializable] internal sealed class RecipeMetadata { public MaterialRecipe[] materials; public string asset_base; }

    /// <summary>Owns generated materials and textures. Create and Dispose on Unity's main thread.</summary>
    public sealed class FoliageMaterials : IDisposable
    {
        public Material[] Materials { get; private set; }
        private readonly List<Texture2D> textures = new List<Texture2D>();
        private readonly Dictionary<string, Texture2D> cache = new Dictionary<string, Texture2D>();
        private bool[] normalMaps;

        public static FoliageMaterials Create(string infoJson, string assetBase, Shader shader = null)
        {
            shader = shader != null ? shader : Resources.Load<Shader>("Foliage");
            if (shader == null) throw new InvalidOperationException("FoliageUtil/Foliage shader is missing. Include the package shader or assign a compatible override.");
            var result = new FoliageMaterials();
            try
            {
                var recipes = JsonUtility.FromJson<RecipeMetadata>(infoJson).materials;
                result.Materials = new Material[recipes.Length];
                result.normalMaps = new bool[recipes.Length];
                for (int i = 0; i < recipes.Length; ++i)
                {
                    MaterialRecipe recipe = recipes[i];
                    var material = new Material(shader) { name = recipe.name, hideFlags = HideFlags.DontSave };
                    result.Materials[i] = material;
                    material.SetVector("_BaseFactor", Vector(recipe.base_color, Vector4.one));
                    material.SetFloat("_Roughness", recipe.roughness);
                    material.SetFloat("_Metallic", recipe.metallic);
                    material.SetFloat("_Cutoff", recipe.alpha_cutoff);
                    material.SetFloat("_AlphaMode", recipe.alpha_mode == "OPAQUE" ? 0 : recipe.alpha_mode == "MASK" ? 1 : 2);
                    material.SetFloat("_Translucency", recipe.translucency);
                    material.SetVector("_TranslucencyColor", Vector(recipe.translucency_color, Vector4.one));
                    material.SetFloat("_Cull", recipe.double_sided ? 0 : 2);
                    material.SetTexture("_BaseMap", result.Load(assetBase, recipe.base_color_texture, false) ?? Texture2D.whiteTexture);
                    Texture2D normal = result.Load(assetBase, recipe.normal_texture, true);
                    material.SetTexture("_NormalMap", normal != null ? normal : Texture2D.normalTexture);
                    material.SetTexture("_PackedMap", result.Load(assetBase, recipe.metallic_roughness_texture, true) ?? Texture2D.whiteTexture);
                    material.SetFloat("_HasNormal", normal != null ? 1 : 0);
                    result.normalMaps[i] = normal != null;
                    bool blend = recipe.alpha_mode == "BLEND";
                    material.SetFloat("_SrcBlend", blend ? 5 : 1);
                    material.SetFloat("_DstBlend", blend ? 10 : 0);
                    material.SetFloat("_ZWrite", blend ? 0 : 1);
                    material.renderQueue = blend ? 3000 : recipe.alpha_mode == "MASK" ? 2450 : 2000;
                    material.SetOverrideTag("RenderType", blend ? "Transparent" : recipe.alpha_mode == "MASK" ? "TransparentCutout" : "Opaque");
                }
                return result;
            }
            catch { result.Dispose(); throw; }
        }
        public void SetNormalMappingEnabled(bool enabled) { for (int i = 0; i < Materials.Length; ++i) Materials[i].SetFloat("_HasNormal", enabled && normalMaps[i] ? 1 : 0); }

        private Texture2D Load(string assetBase, string relative, bool linear)
        {
            if (string.IsNullOrEmpty(relative)) return null;
            string path = Path.GetFullPath(Path.Combine(assetBase, relative));
            string key = path + (linear ? "|data" : "|color");
            if (cache.TryGetValue(key, out Texture2D existing)) return existing;
            if (new FileInfo(path).Length > 64 * 1024 * 1024) throw new InvalidOperationException("Texture exceeds 64 MiB: " + path);
            var image = new Texture2D(2, 2, TextureFormat.RGBA32, true, linear) { name = Path.GetFileName(path), hideFlags = HideFlags.DontSave, wrapMode = TextureWrapMode.Repeat, filterMode = FilterMode.Trilinear, anisoLevel = 4 };
            textures.Add(image);
            if (!image.LoadImage(File.ReadAllBytes(path), true)) throw new InvalidOperationException("Cannot decode texture: " + path);
            cache.Add(key, image);
            return image;
        }
        private static Vector4 Vector(float[] values, Vector4 fallback) { if (values == null) return fallback; return new Vector4(values[0], values[1], values[2], values.Length > 3 ? values[3] : 1); }
        internal static void Destroy(UnityEngine.Object value) { if (value == null) return; if (Application.isPlaying) UnityEngine.Object.Destroy(value); else UnityEngine.Object.DestroyImmediate(value); }
        public void Dispose()
        {
            if (Materials != null) foreach (Material material in Materials) Destroy(material);
            foreach (Texture2D texture in textures) Destroy(texture);
            Materials = Array.Empty<Material>(); textures.Clear(); cache.Clear();
        }
    }
}
