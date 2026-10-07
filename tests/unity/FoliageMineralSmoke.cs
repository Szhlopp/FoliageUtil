using System;
using System.IO;
using System.Linq;
using FoliageUtil;
using UnityEditor;
using UnityEngine;

public static class FoliageMineralSmoke
{
    static FoliageGenerator generator;
    static FoliageRecipe recipe;
    static MeshSnapshot expected;
    static string sample, output;
    static double started;
    static int stage;
    [Serializable] class Generation { public int seed; }
    [Serializable] class Stats { public Generation generation; }
    [Serializable] class Result { public bool verified=true, refractive_shader=false; public int vertices, triangles; public double worker_ms, upload_ms, load_ms; }
    public static void Run()
    {
        try
        {
            PlayerSettings.colorSpace=ColorSpace.Linear;
            var args=Environment.GetCommandLineArgs(); sample=args[Array.IndexOf(args,"-foliageSample")+1];
            output=Path.GetFullPath(Path.Combine(Application.dataPath,"../Artifacts")); Directory.CreateDirectory(output);
            FoliageSmoke.CheckCulling();
            string path=Path.Combine(Application.streamingAssetsPath,"FoliageUtil/"+sample+"/graph.json");
            recipe=new FoliageRecipe(File.ReadAllText(path),Path.GetDirectoryName(path)); expected=recipe.Generate(1,42);
            generator=new GameObject("Live mineral").AddComponent<FoliageGenerator>(); generator.RecipePath="FoliageUtil/"+sample+"/graph.json"; generator.Growth=1; generator.Seed=42; generator.RequestRebuild();
            started=EditorApplication.timeSinceStartup; EditorApplication.update+=Tick;
        }
        catch(Exception error) { Fail(error); }
    }
    static void Tick()
    {
        try
        {
            if(EditorApplication.timeSinceStartup-started>120) throw new Exception("Mineral generation timed out");
            EditorApplication.QueuePlayerLoopUpdate();
            if(generator.LastError!=null) throw new Exception(generator.LastError);
            if(generator.IsBuilding||generator.LastStatisticsJson==null) return;
            int seed=stage==1?77:42;
            if(JsonUtility.FromJson<Stats>(generator.LastStatisticsJson).generation.seed!=seed) return;
            var mesh=generator.GetComponent<MeshFilter>().sharedMesh;
            if(mesh.vertexCount!=expected.VertexCount||expected.TriangleCount==0) throw new Exception("Mineral topology differs from native snapshot");
            var positions=mesh.vertices; var normals=mesh.normals;
            for(int i=0;i<positions.Length;++i) { int n=i*14; if(Vector3.Distance(positions[i],new Vector3(expected.Vertices[n],expected.Vertices[n+1],expected.Vertices[n+2]))>1e-6f||Vector3.Distance(normals[i],new Vector3(expected.Vertices[n+3],expected.Vertices[n+4],expected.Vertices[n+5]))>1e-6f) throw new Exception("Mineral positions or normals differ after upload"); }
            for(int i=0;i<expected.Submeshes.Length;++i) { var range=expected.Submeshes[i]; if(!mesh.GetIndices(i).SequenceEqual(expected.Indices.Skip((int)range.IndexStart).Take((int)range.IndexCount))) throw new Exception("Mineral submesh binding mismatch"); }
            if(mesh.tangents.Length!=mesh.vertexCount||mesh.tangents.Any(t=>float.IsNaN(t.x)||float.IsInfinity(t.x)||Math.Abs(t.w)!=1)) throw new Exception("Invalid mineral tangent frame");
            if(stage<2)
            {
                int next=stage==0?77:42; var nextMesh=recipe.Generate(1,next);
                if(sample!="gems"&&expected.Vertices.SequenceEqual(nextMesh.Vertices)) throw new Exception("Mineral seed did not change geometry");
                expected=nextMesh; generator.Seed=next; generator.RequestRebuild(); ++stage; return;
            }
            var errors=ShaderUtil.GetShaderMessages(Resources.Load<Shader>("Foliage")).Where(m=>m.severity.ToString()=="Error").ToArray();
            if(errors.Length>0) throw new Exception(string.Join("\n",errors.Select(e=>e.message)));
            Render();
            File.WriteAllText(Path.Combine(output,"mineral-smoke.json"),JsonUtility.ToJson(new Result { vertices=mesh.vertexCount,triangles=expected.TriangleCount,worker_ms=generator.LastBuildMilliseconds,upload_ms=generator.LastUploadMilliseconds,load_ms=generator.LastLoadMilliseconds },true));
            recipe.Dispose(); EditorApplication.update-=Tick; Debug.Log("FOLIAGE_UNITY_SMOKE_PASSED"); EditorApplication.Exit(0);
        }
        catch(Exception error) { Fail(error); }
    }
    static void Render()
    {
        RenderSettings.ambientMode=UnityEngine.Rendering.AmbientMode.Flat; RenderSettings.ambientLight=new Color(.35f,.38f,.42f);
        var sun=new GameObject("Sun").AddComponent<Light>(); sun.type=LightType.Directional; sun.intensity=2.2f; sun.transform.rotation=Quaternion.Euler(40,-30,0);
        var camera=new GameObject("Camera").AddComponent<Camera>(); var bounds=generator.GetComponent<MeshRenderer>().bounds;
        camera.orthographic=true; camera.orthographicSize=Mathf.Max(bounds.extents.y,bounds.extents.x)*1.25f; camera.farClipPlane=1000; camera.clearFlags=CameraClearFlags.SolidColor; camera.backgroundColor=new Color(.08f,.1f,.12f);
        var target=new RenderTexture(640,640,24); camera.targetTexture=target; var pixels=new Texture2D(640,640,TextureFormat.RGB24,false);
        for(int view=0;view<2;++view)
        {
            camera.transform.position=bounds.center+new Vector3(view==0?1:-1,.55f,view==0?-3:3).normalized*bounds.size.magnitude*2; camera.transform.LookAt(bounds.center);
            if(UnityEngine.Rendering.GraphicsSettings.defaultRenderPipeline==null) camera.Render(); else UnityEngine.Rendering.RenderPipeline.SubmitRenderRequest(camera,new UnityEngine.Rendering.RenderPipeline.StandardRequest { destination=target });
            RenderTexture.active=target; pixels.ReadPixels(new Rect(0,0,640,640),0,0); pixels.Apply(); File.WriteAllBytes(Path.Combine(output,"mineral-"+view+".png"),pixels.EncodeToPNG());
        }
        RenderTexture.active=null; camera.targetTexture=null; UnityEngine.Object.DestroyImmediate(pixels); UnityEngine.Object.DestroyImmediate(target);
    }
    static void Fail(Exception error) { recipe?.Dispose(); Debug.LogException(error); EditorApplication.update-=Tick; EditorApplication.Exit(1); }
}
