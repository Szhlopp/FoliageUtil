using FoliageUtil;
using UnityEngine;

// Kept under the original class name so existing demo scene references survive.
public sealed class SakuraDemo : MonoBehaviour
{
    public FoliageGenerator Generator;
    public FoliageGenerator Bamboo;
    public float Duration = 18;
    private bool playing;
    private float progress = 1;
    private float nextRequest;
    private int selection;

    private bool IncludesSakura => selection != 2 && Generator != null;
    private bool IncludesBamboo => selection != 1 && Bamboo != null;
    private bool CanGrow => (IncludesSakura || IncludesBamboo) && (!IncludesSakura || string.IsNullOrEmpty(Generator.CardBakeProfile)) && (!IncludesBamboo || string.IsNullOrEmpty(Bamboo.CardBakeProfile));
    private float CurrentGrowth => IncludesSakura ? Generator.Growth : IncludesBamboo ? Bamboo.Growth : 1;

    public void SetProgress(float value)
    {
        progress = Mathf.Clamp01(value);
        if (IncludesSakura) Generator.SetGrowth(progress);
        if (IncludesBamboo) Bamboo.SetGrowth(progress);
    }

    private void Update()
    {
        if (!playing) return;
        if (!CanGrow) { playing = false; return; }
        progress = Mathf.Min(1, progress + Time.deltaTime / Mathf.Max(1, Duration));
        if (Time.unscaledTime >= nextRequest || progress >= 1)
        {
            SetProgress(progress);
            nextRequest = Time.unscaledTime + .25f;
        }
        if (progress >= 1) playing = false;
    }

    private static void Status(string label, FoliageGenerator plant)
    {
        if (plant == null) return;
        GUILayout.Label(label + (plant.IsBuilding ? " | Growing..." : " | Ready"));
        GUILayout.Label($"Worker {plant.LastBuildMilliseconds:F1} ms | Apply {plant.LastUploadMilliseconds:F1} ms");
        if (!string.IsNullOrEmpty(plant.LastError)) GUILayout.Label(plant.LastError);
    }

    private void OnGUI()
    {
        GUILayout.BeginArea(new Rect(16, 16, 300, 340), GUI.skin.box);
        GUILayout.Label("FoliageUtil | Growth garden");
        int next = GUILayout.Toolbar(selection, new[] { "Both", "Sakura", "Bamboo" });
        if (next != selection) { selection = next; playing = false; progress = CurrentGrowth; }
        if (IncludesSakura) Status("Sakura", Generator);
        if (IncludesBamboo) Status("Bamboo", Bamboo);
        GUI.enabled = CanGrow;
        GUILayout.Label($"Growth: {CurrentGrowth:P0}");
        float value = GUILayout.HorizontalSlider(CurrentGrowth, 0, 1);
        if (Mathf.Abs(value - CurrentGrowth) > .001f) { playing = false; SetProgress(value); }
        if (GUILayout.Button("Grow again")) { SetProgress(0); playing = true; }
        if (GUILayout.Button(playing ? "Pause" : "Continue growth")) { progress = CurrentGrowth; playing = !playing; }
        if (GUILayout.Button("Fully grown")) { playing = false; SetProgress(1); }
        GUI.enabled = true;
        GUILayout.Label(CanGrow ? "Select a plant in the Inspector for its recipe and generation settings." : "Clear the selected plant's Card Bake Profile to animate live geometry.");
        GUILayout.EndArea();
    }
}
