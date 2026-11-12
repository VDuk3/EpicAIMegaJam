// trim_plugins.js - Rewrites the "Plugins" list of Glyphbound.uproject:
//   - enables only the Editor toolsets we use (instead of the AllToolsets aggregator),
//   - disables default-enabled engine plugins the game and the workflow do not need,
//   - never disables a plugin that an enabled plugin depends on (checked against every .uplugin).
// Usage: node Tools/Jam/trim_plugins.js "C:/Program Files/Epic Games/UE_5.9" Glyphbound/Glyphbound.uproject
// Re-run after editing the lists below. Reason and measurements: Devlog/days/2026-11-12.md.
const fs = require('fs');
const path = require('path');
const engineDir = process.argv[2];
const uprojectPath = process.argv[3];

// Editor-only plugins we want on.
const ENABLE_EDITOR = [
  'ModelContextProtocol',
  'EditorToolset', 'BlueprintToolsets', 'FileSystemToolset', 'ConsoleToolset', 'ConfigSettingsToolset',
  'AutomationTestToolset', 'LiveCodingToolset', 'InputToolset', 'NavMeshToolset', 'NiagaraToolsets',
  'AIModuleToolset', 'PluginToolset', 'PerfToolset', 'SceneCaptureToolset', 'PhysicsToolsets',
  'CheatsToolset', 'GameplayTagsToolset', 'MetaSoundToolset',
];

// Default-enabled plugins we do not use (single player, Windows only, no cloth, no online, no mobile...).
const DISABLE_CANDIDATES = [
  // Other platforms / mobile / stores
  'AndroidMedia', 'AvfMedia', 'AndroidDeviceProfileSelector', 'AndroidFileServer', 'AndroidMoviePlayer',
  'AndroidPermission', 'AppleImageUtils', 'AppleMoviePlayer', 'GoogleCloudMessaging', 'GooglePAD',
  'IOSDeviceProfileSelector', 'LinuxDeviceProfileSelector', 'MobilePatchingUtils', 'MacGraphicsSwitching',
  'MobileLauncherProfileWizard', 'OnlineSubsystemGooglePlay', 'XCodeSourceCodeAccess', 'ExampleDeviceProfileSelector',
  // Online (single player only)
  'OnlineSubsystem', 'OnlineSubsystemNull', 'OnlineSubsystemUtils', 'OnlineBase', 'ChunkDownloader',
  'LauncherChunkInstaller', 'MsQuic', 'OodleNetwork',
  // Source control and IDEs we do not use (git stays)
  'PerforceSourceControl', 'PlasticSourceControl', 'SubversionSourceControl', 'LoreSourceControl',
  'CLionSourceCodeAccess', 'CodeLiteSourceCodeAccess', 'KDevelopSourceCodeAccess', 'N10XSourceCodeAccess',
  'RiderSourceCodeAccess', 'NullSourceCodeAccess', 'ChangelistReview',
  // Profilers / debug tools not needed for a jam
  'AudioInsights', 'AudioInsightsRuntime', 'AudioModulationInsights', 'ChaosInsights', 'ChaosVD',
  'HttpInsights', 'IoStoreInsights', 'MassInsights', 'NiagaraInsights', 'RenderGraphInsights',
  'RenderTraceInsights', 'LowLevelNetTrace', 'PixWinPlugin', 'RenderDocPlugin', 'GPUReshapeEditor',
  'AutomatedPerfTestTools', 'EditorPerformance', 'EditorTelemetry', 'RuntimeTelemetry', 'WorldMetrics',
  'DumpGPUServices', 'BackChannel', 'CmdLinkServer',
  // Content pipelines we do not use
  'Bridge', 'Fab', 'DatasmithContent', 'GLTFExporter', 'VariantManagerContent', 'AlembicImporter',
  'SpeedTreeImporter', 'InterchangeAxF', 'InterchangeTests', 'MetaHumanSDK', 'MutableAssetUserData',
  'SkeletalReduction', 'FacialAnimation', 'CharacterAI', 'ControlRigPhysics',
  // Features the game does not use
  'ChaosCloth', 'PCG', 'Cascade', 'NiagaraSimCaching', 'NFORDenoise', 'NNEDenoiser', 'MediaPlate',
  'MediaCompositing', 'WebMMoviePlayer', 'ArchVisCharacter', 'AudioCapture', 'CableComponent',
  'CustomMeshComponent', 'GeometryCache', 'LocationServicesBPLibrary', 'ResonanceAudio', 'SoundFields',
  'Synthesis', 'GameplayCameras', 'CameraShakePreviewer', 'AnimationSharing', 'ActorSequence',
  'SequenceValidator', 'SequenceNavigator', 'LevelSequenceNavigatorBridge', 'ColorGrading', 'CryptoKeys',
  'BlueprintHeaderView', 'MaterialAnalyzer', 'AdvancedRenamer', 'WorldPartitionHLODUtilities',
  'PortableObjectFileDataSource', 'XGEController', 'FastBuildController',
];

// ---- read every engine .uplugin ----
const root = path.join(engineDir, 'Engine', 'Plugins');
const plugins = {};
(function walk(dir, depth) {
  let entries;
  try { entries = fs.readdirSync(dir, { withFileTypes: true }); } catch { return; }
  const up = entries.find(e => e.isFile() && e.name.endsWith('.uplugin'));
  if (up) {
    try {
      const j = JSON.parse(fs.readFileSync(path.join(dir, up.name), 'utf8').replace(/^\uFEFF/, ''));
      plugins[path.basename(up.name, '.uplugin')] = {
        def: j.EnabledByDefault === true,
        deps: (j.Plugins || []).filter(p => p.Enabled !== false && !p.Optional).map(p => p.Name),
      };
    } catch { /* unreadable descriptor: ignore */ }
    return;
  }
  if (depth > 6) return;
  for (const e of entries) if (e.isDirectory()) walk(path.join(dir, e.name), depth + 1);
})(root, 0);

const unknown = [...ENABLE_EDITOR, ...DISABLE_CANDIDATES].filter(n => !plugins[n]);
const disable = new Set(DISABLE_CANDIDATES.filter(n => plugins[n] && plugins[n].def));
const notDefault = DISABLE_CANDIDATES.filter(n => plugins[n] && !plugins[n].def);

// Fixpoint: whatever stays enabled keeps its dependencies enabled.
const kept = [];
for (;;) {
  const enabled = new Set(ENABLE_EDITOR.filter(n => plugins[n]));
  for (const [n, p] of Object.entries(plugins)) if (p.def && !disable.has(n)) enabled.add(n);
  const stack = [...enabled];
  const why = {};
  while (stack.length) {
    const n = stack.pop();
    for (const d of (plugins[n] ? plugins[n].deps : [])) {
      if (!why[d]) why[d] = n;
      if (!enabled.has(d)) { enabled.add(d); stack.push(d); }
    }
  }
  const clash = [...disable].filter(n => enabled.has(n));
  if (!clash.length) break;
  for (const n of clash) { disable.delete(n); kept.push(`${n} (needed by ${why[n]})`); }
}

const uproject = JSON.parse(fs.readFileSync(uprojectPath, 'utf8'));
uproject.Plugins = [
  ...ENABLE_EDITOR.filter(n => plugins[n]).map(n => ({ Name: n, Enabled: true, TargetAllowList: ['Editor'] })),
  ...[...disable].sort().map(n => ({ Name: n, Enabled: false })),
];
fs.writeFileSync(uprojectPath, JSON.stringify(uproject, null, '\t') + '\n');
console.log(`enabled (editor only): ${ENABLE_EDITOR.filter(n => plugins[n]).length}`);
console.log(`disabled: ${disable.size}`);
if (kept.length) console.log('kept on because something enabled needs them:\n  ' + kept.join('\n  '));
if (notDefault.length) console.log('already off by default (skipped): ' + notDefault.join(', '));
if (unknown.length) console.log('not found in this engine (skipped): ' + unknown.join(', '));
