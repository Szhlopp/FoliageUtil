using System;
using System.IO;
using System.Threading.Tasks;
using FoliageUtil;
using UnityEngine;

public static class FoliagePlayerSmoke
{
    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    static async void Run()
    {
        string output=Path.Combine(Application.temporaryCachePath,"foliage-player-bake-"+Guid.NewGuid().ToString("N"));
        try
        {
            string path=Path.Combine(Application.streamingAssetsPath,"FoliageUtil/strand/graph.json");
            using(var recipe=new FoliageRecipe(File.ReadAllText(path),Path.GetDirectoryName(path)))
            {
                var empty=await Task.Run(()=>recipe.Generate(0));
                var full=await Task.Run(()=>recipe.Generate(1));
                var tangents=await Task.Run(()=>full.CalculateTangents());
                var baked=await Task.Run(()=>recipe.BakeCards("Strand",output));
                if(empty.VertexCount!=0||full.TriangleCount!=1240||baked.TriangleCount!=24||Array.Exists(tangents,float.IsNaN)) throw new Exception("Player native geometry or bake mismatch");
                Debug.Log("FOLIAGE_PLAYER_SMOKE_PASSED: 1240 geometry triangles, 24 baked triangles");
            }
            Directory.Delete(output,true);Application.Quit(0);
        }
        catch(Exception error) { Debug.LogException(error);Application.Quit(1); }
    }
}
