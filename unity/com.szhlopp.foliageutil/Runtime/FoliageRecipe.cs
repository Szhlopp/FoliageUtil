using System;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32.SafeHandles;

namespace FoliageUtil
{
    public sealed class GenerationLimits
    {
        public uint MaxVertices = 4000000;
        public uint MaxTriangles = 4000000;
        public uint MaxPoints = 500000;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SubmeshRange
    {
        public uint IndexStart;
        public uint IndexCount;
        public uint MaterialIndex;
    }

    public sealed class MeshSnapshot
    {
        // Unity layout: position XYZ, normal XYZ, color RGBA, UV XY, wind XY.
        public readonly float[] Vertices;
        public readonly int[] Indices;
        public readonly SubmeshRange[] Submeshes;
        public readonly string StatisticsJson;
        public readonly string InfoJson;
        public int VertexCount => Vertices.Length / 14;
        public int TriangleCount => Indices.Length / 3;
        internal MeshSnapshot(float[] vertices, int[] indices, SubmeshRange[] submeshes, string statisticsJson, string infoJson) { Vertices = vertices; Indices = indices; Submeshes = submeshes; StatisticsJson = statisticsJson; InfoJson = infoJson; }

        /// <summary>Build tangent-space frames on a worker thread before uploading to Unity.</summary>
        public float[] CalculateTangents()
        {
            var tangents = new float[checked(VertexCount * 4)];
            var bitangents = new float[checked(VertexCount * 3)];
            for (int face = 0; face < Indices.Length; face += 3)
            {
                int a = Indices[face] * 14, b = Indices[face + 1] * 14, c = Indices[face + 2] * 14;
                float ux = Vertices[b + 10] - Vertices[a + 10], uy = Vertices[b + 11] - Vertices[a + 11];
                float vx = Vertices[c + 10] - Vertices[a + 10], vy = Vertices[c + 11] - Vertices[a + 11];
                float determinant = ux * vy - uy * vx;
                if (Math.Abs(determinant) < 1e-12f) continue;
                float inverse = 1 / determinant;
                for (int axis = 0; axis < 3; ++axis)
                {
                    float u = Vertices[b + axis] - Vertices[a + axis], v = Vertices[c + axis] - Vertices[a + axis];
                    float t = (u * vy - v * uy) * inverse, bt = (v * ux - u * vx) * inverse;
                    for (int corner = 0; corner < 3; ++corner)
                    {
                        int vertex = Indices[face + corner];
                        tangents[vertex * 4 + axis] += t; bitangents[vertex * 3 + axis] += bt;
                    }
                }
            }
            for (int vertex = 0; vertex < VertexCount; ++vertex)
            {
                int n = vertex * 14 + 3, t = vertex * 4, b = vertex * 3;
                float nx = Vertices[n], ny = Vertices[n + 1], nz = Vertices[n + 2];
                float dot = nx * tangents[t] + ny * tangents[t + 1] + nz * tangents[t + 2];
                float tx = tangents[t] - nx * dot, ty = tangents[t + 1] - ny * dot, tz = tangents[t + 2] - nz * dot;
                float length = (float)Math.Sqrt(tx * tx + ty * ty + tz * tz);
                if (length < 1e-12f)
                {
                    if (Math.Abs(ny) < .9f) { tx = nz; ty = 0; tz = -nx; }
                    else { tx = 0; ty = -nz; tz = ny; }
                    length = (float)Math.Sqrt(tx * tx + ty * ty + tz * tz);
                    if (length < 1e-12f) { tx = 1; ty = 0; tz = 0; length = 1; }
                }
                tx /= length; ty /= length; tz /= length;
                tangents[t] = tx; tangents[t + 1] = ty; tangents[t + 2] = tz;
                tangents[t + 3] = (ny * tz - nz * ty) * bitangents[b] + (nz * tx - nx * tz) * bitangents[b + 1] + (nx * ty - ny * tx) * bitangents[b + 2] < 0 ? -1 : 1;
            }
            return tangents;
        }
    }

    /// <summary>Owns a parsed native graph. Generate is synchronous and may run on a worker thread.</summary>
    public sealed class FoliageRecipe : IDisposable
    {
        private readonly RecipeHandle handle;
        public string InfoJson { get; }
        public const uint ApiVersion = 1;

        public FoliageRecipe(string json, string assetBase)
        {
            ValidateString(json, nameof(json));
            ValidateString(assetBase, nameof(assetBase));
            uint version = Native.fu_api_version();
            if (version != ApiVersion) throw new NotSupportedException($"FoliageUtil native API {version} does not match wrapper API {ApiVersion}.");
            handle = new RecipeHandle(Native.fu_recipe_create(json, assetBase));
            if (handle.IsInvalid) { handle.Dispose(); throw Native.Error(); }
            try { InfoJson = Native.Text(Native.fu_recipe_info(handle.DangerousGetHandle())); }
            catch { handle.Dispose(); throw; }
        }

        public MeshSnapshot Generate(double progress = 1, int? seed = null, string source = null, string lod = null, GenerationLimits limits = null)
        {
            if (handle.IsClosed) throw new ObjectDisposedException(nameof(FoliageRecipe));
            if (double.IsNaN(progress) || double.IsInfinity(progress) || (progress != -1 && (progress < 0 || progress > 1))) throw new ArgumentOutOfRangeException(nameof(progress));
            if (source != null) ValidateString(source, nameof(source));
            if (lod != null) ValidateString(lod, nameof(lod));
            limits = limits ?? new GenerationLimits();
            IntPtr nativeMesh;
            bool retained = false;
            try
            {
                handle.DangerousAddRef(ref retained);
                nativeMesh = Native.fu_generate(handle.DangerousGetHandle(), source, lod, seed.GetValueOrDefault(), seed.HasValue ? 1 : 0, progress, limits.MaxVertices, limits.MaxTriangles, limits.MaxPoints);
            }
            finally { if (retained) handle.DangerousRelease(); }
            return CopyMesh(nativeMesh);
        }

        /// <summary>Fit cards and bake PNGs into an empty output directory. Reuse the returned snapshot between updates.</summary>
        public MeshSnapshot BakeCards(string profile, string directory, double progress = 1, int? seed = null, string lod = null, GenerationLimits limits = null)
        {
            ValidateString(profile, nameof(profile)); ValidateString(directory, nameof(directory));
            if (lod != null) ValidateString(lod, nameof(lod));
            if (handle.IsClosed) throw new ObjectDisposedException(nameof(FoliageRecipe));
            limits = limits ?? new GenerationLimits();
            IntPtr nativeMesh; bool retained = false;
            try
            {
                handle.DangerousAddRef(ref retained);
                nativeMesh = Native.fu_bake_cards(handle.DangerousGetHandle(), profile, directory, lod, seed.GetValueOrDefault(), seed.HasValue ? 1 : 0, progress, limits.MaxVertices, limits.MaxTriangles, limits.MaxPoints);
            }
            finally { if (retained) handle.DangerousRelease(); }
            return CopyMesh(nativeMesh);
        }

        /// <summary>Bake mature textures once. The returned owner generates growth without rebaking and outlives this recipe.</summary>
        public FoliageCardGrowth PrepareCardGrowth(string profile, string directory, int? seed = null, string lod = null, GenerationLimits limits = null)
        {
            ValidateString(profile, nameof(profile)); ValidateString(directory, nameof(directory));
            if (lod != null) ValidateString(lod, nameof(lod));
            if (handle.IsClosed) throw new ObjectDisposedException(nameof(FoliageRecipe));
            limits = limits ?? new GenerationLimits();
            IntPtr prepared; bool retained = false;
            try
            {
                handle.DangerousAddRef(ref retained);
                prepared = Native.fu_card_growth_create(handle.DangerousGetHandle(), profile, directory, lod, seed.GetValueOrDefault(), seed.HasValue ? 1 : 0, limits.MaxVertices, limits.MaxTriangles, limits.MaxPoints);
            }
            finally { if (retained) handle.DangerousRelease(); }
            return new FoliageCardGrowth(prepared);
        }

        internal static MeshSnapshot CopyMesh(IntPtr nativeMesh)
        {
            using (var mesh = new MeshHandle(nativeMesh))
            {
                if (mesh.IsInvalid) throw Native.Error();
                if (Native.fu_mesh_get_view(mesh.DangerousGetHandle(), out NativeView view) == 0) throw Native.Error();
                if (view.Stride != 14 || view.SubmeshCount > 256 || view.IndexCount % 3 != 0) throw new InvalidOperationException("Invalid native mesh layout.");
                var vertices = new float[checked((int)view.VertexCount * 14)];
                var indices = new int[checked((int)view.IndexCount)];
                var submeshes = new SubmeshRange[checked((int)view.SubmeshCount)];
                if (vertices.Length != 0) Marshal.Copy(view.Vertices, vertices, 0, vertices.Length);
                if (indices.Length != 0) Marshal.Copy(view.Indices, indices, 0, indices.Length);
                for (int i = 0; i < submeshes.Length; ++i) submeshes[i] = Marshal.PtrToStructure<SubmeshRange>(IntPtr.Add(view.Submeshes, checked(i * 12)));
                // Reflect X once to convert the engine's right-handed frame to Unity.
                // Reverse triangle winding to preserve the visible front surface.
                for (int i = 0; i < vertices.Length; i += 14) { vertices[i] = -vertices[i]; vertices[i + 3] = -vertices[i + 3]; }
                for (int i = 0; i < indices.Length; i += 3) { int swap = indices[i + 1]; indices[i + 1] = indices[i + 2]; indices[i + 2] = swap; }
                return new MeshSnapshot(vertices, indices, submeshes, Native.Text(Native.fu_mesh_stats(mesh.DangerousGetHandle())), Native.Text(Native.fu_mesh_info(mesh.DangerousGetHandle())));
            }
        }

        public void Dispose() { handle.Dispose(); }
        private static void ValidateString(string value, string name) { if (value == null) throw new ArgumentNullException(name); if (value.IndexOf('\0') >= 0) throw new ArgumentException("Embedded NUL is not supported.", name); }
    }

    /// <summary>Immutable mature card bindings. Generate may run concurrently on workers. The caller owns the texture directory.</summary>
    public sealed class FoliageCardGrowth : IDisposable
    {
        private readonly CardGrowthHandle handle;
        internal FoliageCardGrowth(IntPtr prepared)
        {
            handle = new CardGrowthHandle(prepared);
            if (handle.IsInvalid) { handle.Dispose(); throw Native.Error(); }
        }
        public MeshSnapshot Generate(double progress, GenerationLimits limits = null)
        {
            if (handle.IsClosed) throw new ObjectDisposedException(nameof(FoliageCardGrowth));
            if (double.IsNaN(progress) || double.IsInfinity(progress) || progress < 0 || progress > 1) throw new ArgumentOutOfRangeException(nameof(progress));
            limits = limits ?? new GenerationLimits();
            IntPtr mesh; bool retained = false;
            try
            {
                handle.DangerousAddRef(ref retained);
                mesh = Native.fu_card_growth_generate(handle.DangerousGetHandle(), progress, limits.MaxVertices, limits.MaxTriangles, limits.MaxPoints);
            }
            finally { if (retained) handle.DangerousRelease(); }
            return FoliageRecipe.CopyMesh(mesh);
        }
        public void Dispose() { handle.Dispose(); }
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct NativeView
    {
        public IntPtr Vertices, Indices, Submeshes;
        public uint VertexCount, IndexCount, SubmeshCount, Stride;
    }
    internal sealed class RecipeHandle : SafeHandleZeroOrMinusOneIsInvalid
    {
        public RecipeHandle(IntPtr value) : base(true) { SetHandle(value); }
        protected override bool ReleaseHandle() { Native.fu_recipe_destroy(handle); return true; }
    }
    internal sealed class MeshHandle : SafeHandleZeroOrMinusOneIsInvalid
    {
        public MeshHandle(IntPtr value) : base(true) { SetHandle(value); }
        protected override bool ReleaseHandle() { Native.fu_mesh_destroy(handle); return true; }
    }
    internal sealed class CardGrowthHandle : SafeHandleZeroOrMinusOneIsInvalid
    {
        public CardGrowthHandle(IntPtr value) : base(true) { SetHandle(value); }
        protected override bool ReleaseHandle() { Native.fu_card_growth_destroy(handle); return true; }
    }
    internal static class Native
    {
        private const string Library = "foliage_native";
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern uint fu_api_version();
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] private static extern IntPtr fu_last_error();
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr fu_recipe_create([MarshalAs(UnmanagedType.LPUTF8Str)] string json, [MarshalAs(UnmanagedType.LPUTF8Str)] string assetBase);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void fu_recipe_destroy(IntPtr recipe);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr fu_recipe_info(IntPtr recipe);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr fu_generate(IntPtr recipe, [MarshalAs(UnmanagedType.LPUTF8Str)] string source, [MarshalAs(UnmanagedType.LPUTF8Str)] string lod, int seed, int overrideSeed, double progress, uint maxVertices, uint maxTriangles, uint maxPoints);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr fu_bake_cards(IntPtr recipe, [MarshalAs(UnmanagedType.LPUTF8Str)] string profile, [MarshalAs(UnmanagedType.LPUTF8Str)] string directory, [MarshalAs(UnmanagedType.LPUTF8Str)] string lod, int seed, int overrideSeed, double progress, uint maxVertices, uint maxTriangles, uint maxPoints);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr fu_card_growth_create(IntPtr recipe, [MarshalAs(UnmanagedType.LPUTF8Str)] string profile, [MarshalAs(UnmanagedType.LPUTF8Str)] string directory, [MarshalAs(UnmanagedType.LPUTF8Str)] string lod, int seed, int overrideSeed, uint maxVertices, uint maxTriangles, uint maxPoints);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void fu_card_growth_destroy(IntPtr growth);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr fu_card_growth_generate(IntPtr growth, double progress, uint maxVertices, uint maxTriangles, uint maxPoints);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr fu_mesh_info(IntPtr mesh);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern void fu_mesh_destroy(IntPtr mesh);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern int fu_mesh_get_view(IntPtr mesh, out NativeView view);
        [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] internal static extern IntPtr fu_mesh_stats(IntPtr mesh);
        internal static Exception Error() { return new InvalidOperationException("FoliageUtil: " + Text(fu_last_error())); }
        internal static string Text(IntPtr pointer)
        {
            if (pointer == IntPtr.Zero) throw new InvalidOperationException("FoliageUtil returned a null string.");
            int length = 0;
            while (Marshal.ReadByte(pointer, length) != 0) { length = checked(length + 1); if (length > 32 * 1024 * 1024) throw new InvalidOperationException("Native text exceeds limit."); }
            var bytes = new byte[length];
            Marshal.Copy(pointer, bytes, 0, length);
            return Encoding.UTF8.GetString(bytes);
        }
    }
}
