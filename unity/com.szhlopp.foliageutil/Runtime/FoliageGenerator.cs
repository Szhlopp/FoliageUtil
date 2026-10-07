using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;
using UnityEngine.Rendering;
using Debug = UnityEngine.Debug;

namespace FoliageUtil
{
    [ExecuteAlways, RequireComponent(typeof(MeshFilter), typeof(MeshRenderer))]
    public sealed class FoliageGenerator : MonoBehaviour
    {
        [Tooltip("Path under StreamingAssets, including graph.json from a prepared runtime bundle.")]
        public string RecipePath = "FoliageUtil/strand/graph.json";
        [Range(0, 1)] public float Growth = 1;
        public bool OverrideSeed = true;
        public int Seed = 42;
        [Tooltip("Empty selects the first saved output mesh.")] public string SourceNode = "";
        public string Lod = "base";
        [Tooltip("Empty uses live geometry. A saved CardBake name fits and bakes cards on the worker when settings change; reuse the result between updates.")] public string CardBakeProfile = "";
        [Tooltip("Bake the mature plant once, then reveal and widen its fitted cards with developmental growth. Growth changes reuse the same textures; recipe, seed, LOD or profile changes prepare a new bake.")] public bool ReuseMatureCardBake;
        [Tooltip("Build tangent frames on the worker. Disable to skip normal mapping.")] public bool CalculateTangents = true;
        public Shader ShaderOverride;
        [Min(1)] public int MaxVertices = 4000000, MaxTriangles = 4000000, MaxPoints = 500000;
        public double LastBuildMilliseconds { get; private set; }
        public double LastUploadMilliseconds { get; private set; }
        public double LastLoadMilliseconds { get; private set; }
        public double LastPreparationMilliseconds { get; private set; }
        public int CardBakePreparationCount { get; private set; }
        public string LastStatisticsJson { get; private set; }
        public string LastMeshInfoJson { get; private set; }
        public string LastError { get; private set; }
        public bool IsBuilding => pending != null;
        public string LastBakeDirectory { get; private set; }

        private FoliageRecipe recipe;
        private FoliageMaterials materials;
        private Mesh ownedMesh;
        private Task<BuildResult> pending;
        private int version, pendingVersion;
        private volatile bool requested;
        private bool reloadRequested;
        private string loadedPath, previousSettings;
        private string cacheRoot;
        private Shader loadedShader;
        private FoliageRecipe materialRecipe;
        private FoliageCardGrowth preparedGrowth;
        private FoliageRecipe preparedRecipe;
        private string preparedKey, preparedDirectory;
        private sealed class BuildResult { public MeshSnapshot Mesh; public float[] Tangents; public Bounds Bounds; public double Milliseconds, PreparationMilliseconds; public string BakeDirectory, Key; public FoliageCardGrowth Prepared; public FoliageRecipe Recipe; public bool DidPrepare; }

        private void OnEnable()
        {
            previousSettings = null; RequestRebuild();
            cacheRoot = Path.Combine(Application.temporaryCachePath, "FoliageUtil", Guid.NewGuid().ToString("N"));
#if UNITY_EDITOR
            UnityEditor.EditorApplication.update += EditorTick;
#endif
        }
#if UNITY_EDITOR
        private void EditorTick() { if (pending != null || requested) UnityEditor.EditorApplication.QueuePlayerLoopUpdate(); }
#endif
        private void OnValidate() { RequestRebuild(); }
        public void SetGrowth(float progress) { Growth = Mathf.Clamp01(progress); RequestRebuild(); }
        public void RequestRebuild() { Interlocked.Increment(ref version); requested = true; }
        public void ReloadRecipe() { reloadRequested = true; RequestRebuild(); }

        private void Update()
        {
            string settings = $"{RecipePath}|{Growth:R}|{OverrideSeed}|{Seed}|{SourceNode}|{Lod}|{CardBakeProfile}|{ReuseMatureCardBake}|{CalculateTangents}|{MaxVertices}|{MaxTriangles}|{MaxPoints}|{ShaderOverride}";
            if (settings != previousSettings) { previousSettings = settings; RequestRebuild(); }
            if (pending != null)
            {
                if (!pending.IsCompleted) return;
                Task<BuildResult> completed = pending; pending = null;
                if (completed.IsFaulted) { if (pendingVersion == version) Report(completed.Exception.GetBaseException()); else _ = completed.Exception; }
                else
                {
                    var result = completed.Result;
                    try
                    {
                        var upload = Stopwatch.StartNew(); Apply(result); LastUploadMilliseconds = upload.Elapsed.TotalMilliseconds;
                        if (preparedGrowth != result.Prepared) preparedGrowth?.Dispose();
                        preparedGrowth = result.Prepared; preparedRecipe = result.Recipe; preparedKey = result.Key; preparedDirectory = result.Prepared == null ? null : result.BakeDirectory;
                        if (result.DidPrepare) { LastPreparationMilliseconds = result.PreparationMilliseconds; ++CardBakePreparationCount; }
                        LastBuildMilliseconds = result.Milliseconds; LastStatisticsJson = result.Mesh.StatisticsJson; LastError = null;
                    }
                    catch (Exception error)
                    {
                        if (result.Prepared != preparedGrowth) result.Prepared?.Dispose();
                        if (result.BakeDirectory != LastBakeDirectory && result.BakeDirectory != preparedDirectory) DeleteCache(result.BakeDirectory);
                        Report(error);
                    }
                }
            }
            if (!requested) return;
            requested = false;
            try
            {
                string path = Path.GetFullPath(Path.Combine(Application.streamingAssetsPath, RecipePath));
                if (recipe == null || reloadRequested || path != loadedPath)
                {
                    var load = Stopwatch.StartNew();
                    var next = new FoliageRecipe(File.ReadAllText(path), Path.GetDirectoryName(path));
                    try
                    {
                        if (materials == null)
                        {
                            materials = FoliageMaterials.Create(next.InfoJson, Path.GetDirectoryName(path), ShaderOverride);
                            materialRecipe = next; loadedShader = ShaderOverride;
                            GetComponent<MeshRenderer>().sharedMaterials = materials.Materials;
                        }
                    }
                    catch { next.Dispose(); throw; }
                    recipe?.Dispose();
                    recipe = next; loadedPath = path;
                    reloadRequested = false;
                    LastLoadMilliseconds = load.Elapsed.TotalMilliseconds;
                }
                FoliageRecipe target = recipe;
                float progress = Growth;
                int? seed = OverrideSeed ? Seed : (int?)null;
                string source = SourceNode, lod = Lod, bake = CardBakeProfile;
                bool reuse = ReuseMatureCardBake && !string.IsNullOrEmpty(bake);
                string key = $"{OverrideSeed}|{Seed}|{lod}|{bake}|{MaxVertices}|{MaxTriangles}|{MaxPoints}";
                FoliageCardGrowth cached = reuse && preparedRecipe == target && preparedKey == key ? preparedGrowth : null;
                string bakeDirectory = cached != null ? preparedDirectory : string.IsNullOrEmpty(bake) ? null : Path.Combine(cacheRoot, Guid.NewGuid().ToString("N"));
                bool tangents = CalculateTangents;
                var limits = new GenerationLimits { MaxVertices = checked((uint)Math.Max(1, MaxVertices)), MaxTriangles = checked((uint)Math.Max(1, MaxTriangles)), MaxPoints = checked((uint)Math.Max(1, MaxPoints)) };
                pendingVersion = version;
                pending = Task.Run(() =>
                {
                    FoliageCardGrowth prepared = cached;
                    try
                    {
                        var preparation = Stopwatch.StartNew(); bool didPrepare = reuse && prepared == null;
                        if (didPrepare) prepared = target.PrepareCardGrowth(bake, bakeDirectory, seed, lod, limits);
                        double preparationMs = didPrepare ? preparation.Elapsed.TotalMilliseconds : 0;
                        var clock = Stopwatch.StartNew();
                        MeshSnapshot mesh = prepared != null ? prepared.Generate(progress, limits) : bakeDirectory == null ? target.Generate(progress, seed, source, lod, limits) : target.BakeCards(bake, bakeDirectory, progress, seed, lod, limits);
                        var result = new BuildResult { Mesh = mesh, Tangents = tangents ? mesh.CalculateTangents() : null, BakeDirectory = bakeDirectory, Prepared = prepared, Recipe = target, Key = key, DidPrepare = didPrepare, PreparationMilliseconds = preparationMs };
                        if (mesh.VertexCount != 0)
                        {
                            var min = new Vector3(float.MaxValue, float.MaxValue, float.MaxValue); var max = -min;
                            for (int i = 0; i < mesh.Vertices.Length; i += 14) { var point = new Vector3(mesh.Vertices[i], mesh.Vertices[i + 1], mesh.Vertices[i + 2]); min = Vector3.Min(min, point); max = Vector3.Max(max, point); }
                            result.Bounds = new Bounds((min + max) * .5f, max - min);
                        }
                        result.Milliseconds = clock.Elapsed.TotalMilliseconds; return result;
                    }
                    catch { if (prepared != cached) prepared?.Dispose(); if (cached == null) DeleteCache(bakeDirectory); throw; }
                });
            }
            catch (Exception error) { Report(error); }
        }

        private void Apply(BuildResult result)
        {
            MeshSnapshot snapshot = result.Mesh;
            if (result.BakeDirectory != LastBakeDirectory || loadedShader != ShaderOverride || materialRecipe != result.Recipe)
            {
                var metadata = JsonUtility.FromJson<RecipeMetadata>(snapshot.InfoJson);
                var nextMaterials = FoliageMaterials.Create(snapshot.InfoJson, metadata.asset_base, ShaderOverride);
                materials?.Dispose(); materials = nextMaterials;
                loadedShader = ShaderOverride; materialRecipe = result.Recipe;
                GetComponent<MeshRenderer>().sharedMaterials = materials.Materials;
                string previous = LastBakeDirectory; LastBakeDirectory = result.BakeDirectory;
                if (previous != null && previous != LastBakeDirectory) _ = Task.Run(() => DeleteCache(previous));
            }
            materials.SetNormalMappingEnabled(result.Tangents != null);
            if (ownedMesh == null) { ownedMesh = new Mesh { name = "FoliageUtil live mesh", hideFlags = HideFlags.DontSave }; ownedMesh.MarkDynamic(); }
            ownedMesh.Clear();
            if (snapshot.VertexCount != 0 && snapshot.Indices.Length != 0)
            {
                ownedMesh.SetVertexBufferParams(snapshot.VertexCount,
                    new VertexAttributeDescriptor(VertexAttribute.Position, VertexAttributeFormat.Float32, 3),
                    new VertexAttributeDescriptor(VertexAttribute.Normal, VertexAttributeFormat.Float32, 3),
                    new VertexAttributeDescriptor(VertexAttribute.Color, VertexAttributeFormat.Float32, 4),
                    new VertexAttributeDescriptor(VertexAttribute.TexCoord0, VertexAttributeFormat.Float32, 2),
                    new VertexAttributeDescriptor(VertexAttribute.TexCoord1, VertexAttributeFormat.Float32, 2),
                    new VertexAttributeDescriptor(VertexAttribute.Tangent, VertexAttributeFormat.Float32, 4, 1));
                ownedMesh.SetVertexBufferData(snapshot.Vertices, 0, 0, snapshot.Vertices.Length);
                if (result.Tangents != null) ownedMesh.SetVertexBufferData(result.Tangents, 0, 0, result.Tangents.Length, 1);
                ownedMesh.SetIndexBufferParams(snapshot.Indices.Length, IndexFormat.UInt32);
                ownedMesh.SetIndexBufferData(snapshot.Indices, 0, 0, snapshot.Indices.Length);
                ownedMesh.subMeshCount = snapshot.Submeshes.Length;
                for (int i = 0; i < snapshot.Submeshes.Length; ++i)
                {
                    SubmeshRange range = snapshot.Submeshes[i];
                    ownedMesh.SetSubMesh(i, new SubMeshDescriptor(checked((int)range.IndexStart), checked((int)range.IndexCount), MeshTopology.Triangles) { bounds = result.Bounds, firstVertex = 0, vertexCount = snapshot.VertexCount }, MeshUpdateFlags.DontRecalculateBounds);
                }
                ownedMesh.bounds = result.Bounds;
            }
            GetComponent<MeshFilter>().sharedMesh = ownedMesh;
            GetComponent<MeshRenderer>().enabled = snapshot.Indices.Length != 0;
            LastMeshInfoJson = snapshot.InfoJson;
        }
        private void Report(Exception error) { LastError = error.Message; Debug.LogError("FoliageUtil: " + error.Message, this); }
        private static void DeleteCache(string path) { if (path == null) return; try { if (Directory.Exists(path)) Directory.Delete(path, true); } catch (IOException) { } catch (UnauthorizedAccessException) { } }
        private void OnDisable()
        {
#if UNITY_EDITOR
            UnityEditor.EditorApplication.update -= EditorTick;
#endif
            Interlocked.Increment(ref version);
            string cleanup = cacheRoot;
            if (pending != null) _ = pending.ContinueWith(task => { if (task.IsFaulted) _ = task.Exception; else if (!task.IsCanceled) task.Result.Prepared?.Dispose(); DeleteCache(cleanup); });
            else _ = Task.Run(() => DeleteCache(cleanup));
            pending = null; recipe?.Dispose(); recipe = null;
            preparedGrowth?.Dispose(); preparedGrowth = null; preparedRecipe = null; preparedKey = preparedDirectory = null;
            LastBakeDirectory = null;
            materials?.Dispose(); materials = null; materialRecipe = null;
            if (ownedMesh != null && GetComponent<MeshFilter>().sharedMesh == ownedMesh) GetComponent<MeshFilter>().sharedMesh = null;
            FoliageMaterials.Destroy(ownedMesh); ownedMesh = null;
        }
    }
}
