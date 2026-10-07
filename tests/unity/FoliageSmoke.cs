using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using FoliageUtil;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

public static class FoliageSmoke
{
    static FoliageGenerator generator;
    static MeshSnapshot full;
    static int stage;
    static string sample = "strand", bake;
    static double started;
    [Serializable] class Stats { public double progress; }
    [Serializable] class Result { public bool verified=true; public int vertices, triangles; public double worker_ms, upload_ms, first_load_ms; }
    public static void Run()
    {
        try
        {
            PlayerSettings.colorSpace=ColorSpace.Linear;
            var args=Environment.GetCommandLineArgs();int arg=Array.IndexOf(args,"-foliageSample");if(arg>=0) sample=args[arg+1];
            int bakeArg=Array.IndexOf(args,"-foliageBake");if(bakeArg>=0) bake=args[bakeArg+1];
            CheckCulling();
            string path=Path.Combine(Application.streamingAssetsPath,"FoliageUtil/"+sample+"/graph.json");
            using(var recipe=new FoliageRecipe(File.ReadAllText(path),Path.GetDirectoryName(path)))
            {
                string temp=Path.Combine(Path.GetTempPath(),"foliage-unity-reference-"+Guid.NewGuid().ToString("N"));
                try { full=bake==null?recipe.Generate(1,42):recipe.BakeCards(bake,temp,1,42); }
                finally { if(Directory.Exists(temp)) Directory.Delete(temp,true); }
            }
            generator=new GameObject("Live native foliage").AddComponent<FoliageGenerator>();
            generator.Growth=0;generator.CardBakeProfile=bake??""; generator.RecipePath="FoliageUtil/"+sample+"/graph.json"; generator.RequestRebuild();
            started=EditorApplication.timeSinceStartup; EditorApplication.update+=Tick;
        }
        catch(Exception error) { Fail(error); }
    }
    internal static void CheckCulling()
    {
        const string json="{\"version\":1,\"materials\":{\"test\":{\"base_color\":[1,1,1,1],\"double_sided\":false}},\"nodes\":{\"card\":{\"op\":\"card\",\"width\":1,\"height\":1,\"material\":\"test\"}},\"outputs\":{\"card.glb\":\"card\"}}";
        using(var recipe=new FoliageRecipe(json,Application.streamingAssetsPath))
        using(var materials=FoliageMaterials.Create(recipe.InfoJson,Application.streamingAssetsPath))
        {
            var snapshot=recipe.Generate();var vertices=new Vector3[snapshot.VertexCount];var normals=new Vector3[vertices.Length];var colors=new Color[vertices.Length];
            for(int i=0;i<vertices.Length;++i) { int n=i*14;vertices[i]=new Vector3(snapshot.Vertices[n],snapshot.Vertices[n+1],snapshot.Vertices[n+2]);normals[i]=new Vector3(snapshot.Vertices[n+3],snapshot.Vertices[n+4],snapshot.Vertices[n+5]);colors[i]=Color.white; }
            var mesh=new Mesh { vertices=vertices,normals=normals,colors=colors,triangles=snapshot.Indices };
            var card=new GameObject("Single sided face test");card.AddComponent<MeshFilter>().sharedMesh=mesh;
            card.AddComponent<MeshRenderer>().sharedMaterial=materials.Materials[snapshot.Submeshes.First(s=>s.IndexCount>0).MaterialIndex];
            var camera=new GameObject("Culling test camera").AddComponent<Camera>();camera.orthographic=true;camera.orthographicSize=.7f;camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=Color.black;
            RenderSettings.ambientMode=UnityEngine.Rendering.AmbientMode.Flat;RenderSettings.ambientLight=Color.white;
            var rt=new RenderTexture(32,32,24);camera.targetTexture=rt;var pixels=new Texture2D(32,32,TextureFormat.RGB24,false);
            float[] visible=new float[2];
            for(int side=0;side<2;++side) { camera.transform.position=mesh.bounds.center+normals[0]*(side==0?3:-3);camera.transform.LookAt(mesh.bounds.center);RenderCamera(camera);RenderTexture.active=rt;pixels.ReadPixels(new Rect(0,0,32,32),0,0);pixels.Apply();visible[side]=pixels.GetPixel(16,16).maxColorComponent; }
            RenderTexture.active=null;camera.targetTexture=null;
            UnityEngine.Object.DestroyImmediate(card);UnityEngine.Object.DestroyImmediate(camera.gameObject);UnityEngine.Object.DestroyImmediate(mesh);UnityEngine.Object.DestroyImmediate(rt);UnityEngine.Object.DestroyImmediate(pixels);
            if(visible[0]<.1f||visible[1]>.01f) throw new Exception($"Native card front/back culling incorrect: front={visible[0]}, back={visible[1]}");
        }
    }
    static void Tick()
    {
        try
        {
            if(EditorApplication.timeSinceStartup-started>120) throw new Exception("Timed out waiting for live generation");
            EditorApplication.QueuePlayerLoopUpdate();
            if(generator.LastError!=null) throw new Exception(generator.LastError);
            if(generator.IsBuilding||generator.LastStatisticsJson==null) return;
            var progress=JsonUtility.FromJson<Stats>(generator.LastStatisticsJson).progress;
            var mesh=generator.GetComponent<MeshFilter>().sharedMesh;
            if(stage==0&&progress==0)
            {
                if(mesh.vertexCount!=0||generator.GetComponent<MeshRenderer>().enabled) throw new Exception("Empty growth retained geometry");
                generator.SetGrowth(.5f);stage=1;
            }
            else if(stage==1&&Math.Abs(progress-.5)<.0001)
            {
                if(mesh.vertexCount==0||(bake==null&&mesh.vertexCount>=full.VertexCount)) throw new Exception("Intermediate growth did not change topology");
                generator.SetGrowth(.6f);generator.SetGrowth(.8f);generator.SetGrowth(1);stage=2;
            }
            else if(stage==2&&progress==1)
            {
                if(mesh.vertexCount!=full.VertexCount) throw new Exception("Vertex count mismatch");
                var verts=mesh.vertices;var normals=mesh.normals;var uv=mesh.uv;var colors=mesh.colors;
                var wind=new List<Vector2>();mesh.GetUVs(1,wind);
                for(int i=0;i<verts.Length;++i)
                {
                    int n=i*14;
                    if(Vector3.Distance(verts[i],new Vector3(full.Vertices[n],full.Vertices[n+1],full.Vertices[n+2]))>1e-6) throw new Exception("Position layout mismatch");
                    if(Math.Abs(uv[i].x-full.Vertices[n+10])>1e-6||Math.Abs(colors[i].r-full.Vertices[n+6])>1e-6||Math.Abs(wind[i].x-full.Vertices[n+12])>1e-6) throw new Exception("UV/color/wind layout mismatch");
                    if(Vector3.Distance(normals[i],new Vector3(full.Vertices[n+3],full.Vertices[n+4],full.Vertices[n+5]))>1e-6) throw new Exception("Normal layout mismatch");
                }
                for(int i=0;i<full.Submeshes.Length;++i)
                {
                    var range=full.Submeshes[i];
                    if(!mesh.GetIndices(i).SequenceEqual(full.Indices.Skip((int)range.IndexStart).Take((int)range.IndexCount))) throw new Exception("Submesh index mismatch");
                }
                var shader=Resources.Load<Shader>("Foliage");
                var errors=ShaderUtil.GetShaderMessages(shader).Where(m=>m.severity.ToString()=="Error").ToArray();
                var tangents=mesh.tangents;
                if(tangents.Length!=mesh.vertexCount||tangents.Any(t=>float.IsNaN(t.x)||Math.Abs(t.w)!=1)) throw new Exception("Worker tangents were not uploaded");
                if(errors.Length>0) throw new Exception(string.Join("\n",errors.Select(e=>e.message)));
                Render();
                string scenePath="Assets/LiveFoliageSaveTest.unity";
                if(!EditorSceneManager.SaveScene(generator.gameObject.scene,scenePath)) throw new Exception("Could not save the live generator scene");
                string sceneText=File.ReadAllText(scenePath);
                if(sceneText.Contains("\nMesh:")||sceneText.Contains("\nTexture2D:")||sceneText.Contains("\nMaterial:")||new FileInfo(scenePath).Length>100000) throw new Exception("Live generated mesh/material/texture data leaked into scene serialization");
                string prefabPath=FoliageUtil.Editor.FoliagePrefabExporter.Save(generator,"Assets/GeneratedFoliage-"+Guid.NewGuid().ToString("N"));
                AssetDatabase.ImportAsset(prefabPath,ImportAssetOptions.ForceSynchronousImport);
                var prefab=AssetDatabase.LoadAssetAtPath<GameObject>(prefabPath);
                if(prefab.GetComponent<FoliageGenerator>()!=null||prefab.GetComponent<MeshFilter>().sharedMesh.vertexCount!=full.VertexCount) throw new Exception("Saved prefab is not a reusable static mesh");
                if(prefab.GetComponent<MeshFilter>().sharedMesh.hideFlags!=HideFlags.None) throw new Exception("Saved mesh inherited temporary flags");
                foreach(var material in prefab.GetComponent<MeshRenderer>().sharedMaterials)
                {
                    if(material.hideFlags!=HideFlags.None) throw new Exception("Saved material inherited temporary flags");
                    foreach(string property in new[]{"_BaseMap","_NormalMap","_PackedMap"})
                    {
                        var texture=material.GetTexture(property);
                        if(texture!=Texture2D.whiteTexture&&texture!=Texture2D.normalTexture&&string.IsNullOrEmpty(AssetDatabase.GetAssetPath(texture))) throw new Exception("Prefab retained a temporary runtime texture");
                    }
                }
                string dir=Path.GetFullPath(Path.Combine(Application.dataPath,"../../unity-preview"));Directory.CreateDirectory(dir);
                File.WriteAllText(Path.Combine(dir,sample+(bake==null?"":"-baked")+"-smoke.json"),JsonUtility.ToJson(new Result { vertices=mesh.vertexCount,triangles=full.TriangleCount,worker_ms=generator.LastBuildMilliseconds,upload_ms=generator.LastUploadMilliseconds,first_load_ms=generator.LastLoadMilliseconds },true));
                UnityEngine.Debug.Log("FOLIAGE_UNITY_SMOKE_PASSED");EditorApplication.update-=Tick;EditorApplication.Exit(0);
            }
        }
        catch(Exception error) { Fail(error); }
    }
    static void Render()
    {
        RenderSettings.ambientMode=UnityEngine.Rendering.AmbientMode.Flat;RenderSettings.ambientLight=new Color(.35f,.38f,.42f);
        var light=new GameObject("Sun").AddComponent<Light>();light.type=LightType.Directional;light.intensity=2.2f;light.transform.rotation=Quaternion.Euler(40,-30,0);light.shadows=LightShadows.Soft;
        var camera=new GameObject("Camera").AddComponent<Camera>();var bounds=generator.GetComponent<MeshRenderer>().bounds;camera.transform.position=bounds.center+new Vector3(1,.25f,-3).normalized*bounds.size.magnitude*2;camera.transform.LookAt(bounds.center);camera.orthographic=true;camera.orthographicSize=Mathf.Max(bounds.extents.y,bounds.extents.x)*1.18f;camera.farClipPlane=1000;camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=new Color(.08f,.1f,.12f);
        var target=new RenderTexture(768,768,24);camera.targetTexture=target;RenderCamera(camera);RenderTexture.active=target;
        var image=new Texture2D(768,768,TextureFormat.RGB24,false);image.ReadPixels(new Rect(0,0,768,768),0,0);image.Apply();
        string dir=Path.GetFullPath(Path.Combine(Application.dataPath,"../../unity-preview"));Directory.CreateDirectory(dir);File.WriteAllBytes(Path.Combine(dir,sample+(bake==null?"":"-baked")+".png"),image.EncodeToPNG());
        RenderTexture.active=null;camera.targetTexture=null;UnityEngine.Object.DestroyImmediate(image);UnityEngine.Object.DestroyImmediate(target);
    }
    static void RenderCamera(Camera camera)
    {
        if(UnityEngine.Rendering.GraphicsSettings.defaultRenderPipeline==null) camera.Render();
        else UnityEngine.Rendering.RenderPipeline.SubmitRenderRequest(camera,new UnityEngine.Rendering.RenderPipeline.StandardRequest { destination=camera.targetTexture });
    }
    static void Fail(Exception error) { UnityEngine.Debug.LogException(error);EditorApplication.update-=Tick;EditorApplication.Exit(1); }
}
