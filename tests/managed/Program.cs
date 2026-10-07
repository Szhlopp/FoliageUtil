using FoliageUtil;
using System.Text.Json;
using System.Diagnostics;
using System.Runtime.InteropServices;

static void Check(bool value, string message) { if (!value) throw new Exception(message); }
string path = Path.GetFullPath(args[0]);
Check(Marshal.SizeOf<SubmeshRange>() == 12, "submesh ABI size");
var recipe = new FoliageRecipe(File.ReadAllText(path), Path.GetDirectoryName(path));
using (var metadata = JsonDocument.Parse(recipe.InfoJson)) Check(metadata.RootElement.GetProperty("api_version").GetInt32() == 1, "metadata version");
var watch = Stopwatch.StartNew();
var empty = recipe.Generate(0);
Check(empty.VertexCount == 0 && empty.TriangleCount == 0, "empty stage");
var full = recipe.Generate(1, 42);
var tangents = full.CalculateTangents();
Check(tangents.Length == full.VertexCount * 4 && tangents.All(float.IsFinite), "finite tangent frames");
for (int i = 0; i < full.VertexCount; ++i)
{
    int t = i * 4, n = i * 14 + 3;
    float length = tangents[t] * tangents[t] + tangents[t + 1] * tangents[t + 1] + tangents[t + 2] * tangents[t + 2];
    float dot = tangents[t] * full.Vertices[n] + tangents[t + 1] * full.Vertices[n + 1] + tangents[t + 2] * full.Vertices[n + 2];
    Check(Math.Abs(length - 1) < .001f && Math.Abs(dot) < .001f && Math.Abs(tangents[t + 3]) == 1, "orthonormal tangent basis");
}
var again = recipe.Generate(1, 42);
Check(full.Vertices.SequenceEqual(again.Vertices) && full.Indices.SequenceEqual(again.Indices), "deterministic managed copy");
Check(full.Indices.All(i => i >= 0 && i < full.VertexCount), "indices in range");
Check(full.Submeshes.Sum(s => (long)s.IndexCount) == full.Indices.Length, "all material ranges");
var jobs = Enumerable.Range(1, 5).Select(i => Task.Run(() => recipe.Generate(i / 5.0))).ToArray();
await Task.WhenAll(jobs);
Check(jobs.All(t => t.Result.Vertices.All(float.IsFinite)), "concurrent generations finite");
string bakeDirectory = Path.Combine(Path.GetTempPath(), "foliage-managed-bake-" + Guid.NewGuid().ToString("N"));
try
{
    MeshSnapshot baked = await Task.Run(() => recipe.BakeCards("Strand", bakeDirectory));
    Check(baked.TriangleCount > 0 && baked.TriangleCount < full.TriangleCount, "managed CardBake reduction");
    using var info = JsonDocument.Parse(baked.InfoJson);
    Check(info.RootElement.GetProperty("materials").GetArrayLength() == baked.Submeshes.Length, "baked material binding");
    Check(Directory.EnumerateFiles(bakeDirectory, "*.png", SearchOption.AllDirectories).Any(), "baked texture package");
    try { recipe.BakeCards("Strand", bakeDirectory); throw new Exception("overwrite should fail"); } catch (InvalidOperationException) { }
}
finally { if (Directory.Exists(bakeDirectory)) Directory.Delete(bakeDirectory, true); }
string reusableDirectory = Path.Combine(Path.GetTempPath(), "foliage-managed-growth-" + Guid.NewGuid().ToString("N"));
try
{
    FoliageCardGrowth prepared;
    using (var owner = new FoliageRecipe(File.ReadAllText(path), Path.GetDirectoryName(path))) prepared = owner.PrepareCardGrowth("Strand", reusableDirectory, 42);
    var package = Directory.GetFiles(reusableDirectory, "*", SearchOption.AllDirectories).ToDictionary(p => p, p => (File.ReadAllBytes(p), File.GetLastWriteTimeUtc(p)));
    using (prepared)
    {
        var matureCards = prepared.Generate(1);
        Check(matureCards.TriangleCount < full.TriangleCount && matureCards.TriangleCount > 0, "cached card reduction");
        Check(prepared.Generate(0).VertexCount == 0, "empty cached growth");
        var samples = await Task.WhenAll(Enumerable.Range(1, 12).Select(i => Task.Run(() => prepared.Generate(i / 12.0))));
        Check(samples.All(m => m.Vertices.All(float.IsFinite) && m.InfoJson == matureCards.InfoJson), "concurrent cached growth with stable materials");
        var middle = prepared.Generate(.53); prepared.Generate(.9);
        Check(middle.Vertices.SequenceEqual(prepared.Generate(.53).Vertices), "cached backward scrubbing");
        Check(middle.CalculateTangents().All(float.IsFinite), "cached partial tangent frames");
        foreach (var entry in package) Check(File.ReadAllBytes(entry.Key).SequenceEqual(entry.Value.Item1) && File.GetLastWriteTimeUtc(entry.Key) == entry.Value.Item2, "cached updates do not write textures");
        try { prepared.Generate(1, new GenerationLimits { MaxTriangles = 1 }); throw new Exception("cached budget should fail"); } catch (InvalidOperationException) { }
    }
    try { prepared.Generate(.5); throw new Exception("disposed cached handle should fail"); } catch (ObjectDisposedException) { }
    var pendingCards = recipe.PrepareCardGrowth("Strand", Path.Combine(reusableDirectory, "lifetime"));
    var task = Task.Run(() => { try { return pendingCards.Generate(.5); } catch (ObjectDisposedException) { return null; } });
    pendingCards.Dispose(); await task;
}
finally { if (Directory.Exists(reusableDirectory)) Directory.Delete(reusableDirectory, true); }
try { recipe.Generate(1, limits: new GenerationLimits { MaxVertices = 1 }); throw new Exception("budget should fail"); }
catch (InvalidOperationException e) { Check(e.Message.Contains("budget"), "native error propagation"); }
recipe.Dispose(); recipe.Dispose();
try { recipe.Generate(); throw new Exception("disposed should fail"); } catch (ObjectDisposedException) { }
try { using var bad = new FoliageRecipe("{", ""); throw new Exception("bad JSON should fail"); } catch (InvalidOperationException) { }
var lifetime = new FoliageRecipe(File.ReadAllText(path), Path.GetDirectoryName(path));
var pending = Task.Run(() => { try { return lifetime.Generate(); } catch (ObjectDisposedException) { return null; } });
lifetime.Dispose(); await pending;
Console.WriteLine(JsonSerializer.Serialize(new { verified = true, vertices = full.VertexCount, triangles = full.TriangleCount, elapsed_ms = watch.Elapsed.TotalMilliseconds }));
foreach (string name in new[] { "rocks", "cliff", "rock-formation", "gems", "crystal-cluster" })
{
    string mineralPath = Path.Combine(Path.GetDirectoryName(path)!, name + ".json");
    using var mineral = new FoliageRecipe(File.ReadAllText(mineralPath), Path.GetDirectoryName(mineralPath));
    var model = mineral.Generate(1, 42);
    Check(model.TriangleCount > 0 && model.Vertices.All(float.IsFinite) && model.CalculateTangents().All(float.IsFinite), "mineral managed buffers and tangents");
    Check(model.Indices.All(i => i >= 0 && i < model.VertexCount), "mineral indices");
    Check(model.Submeshes.Sum(s => (long)s.IndexCount) == model.Indices.Length, "mineral material bindings");
    Check(model.Vertices.SequenceEqual(mineral.Generate(1, 42).Vertices), "mineral managed repeatability");
}
Console.WriteLine("Mineral managed smoke passed");
