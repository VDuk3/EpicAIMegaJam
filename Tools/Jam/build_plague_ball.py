# build_plague_ball.py - rebuilds the final boss's plague ball, /Game/Glyphbound/FX/Reaper/NS_Reaper_PlagueBall, in place.
# Run with: node Tools/Jam/mcp_call.js py Tools/Jam/build_plague_ball.py   (Editor open, no PIE). It does not save; SAVE = True saves.
#
# The look (owner, 2026-11-15): a glowing plague-green core (#5CFF7E) with its light, trailing a REAL Niagara Fluids
# grey smoke: a Grid3D gas sim in world space (Compensate For Actor Motion), so the smoke stays where the ball left it
# and curls and fades on its own. No hand-made cloud shape.
#   - Grid3D_Gas_Master_Emitter: a long box that follows the ball (the ball near its front). Its own sphere source
#     sits on the ball and injects density = FillDensity * User.Alive with a forward push (the air the ball drags);
#     the sim's turbulence and vorticity curl it, and dissipation fades it in about 1.2 s.
#     (A separate particle source emitter, as in the Grid3D_Gas_SimpleParticleSource template, did not reach the grid
#     here, and an emitter copied out of another system keeps a private parent and cannot be saved: not used.)
#   - Core (CPU): green sprite + light; spawns only while User.Alive, killed at once when Alive drops to 0.
# Code contract (GBValkyrieEnemy FireVolley): on impact the component is detached, User.Alive = 0, no Deactivate,
# DestroyComponent 1.5 s later. So the smoke source and core stop on Alive = 0 and the grid keeps fading.
# Idempotent: every value is set each run; a leftover SmokeSource emitter from an earlier version is removed.
import json

SAVE = False
SYS = '/Game/Glyphbound/FX/Reaper/NS_Reaper_PlagueBall.NS_Reaper_PlagueBall'
GAS = 'Grid3D_Gas_Master_Emitter'
SRC = 'SmokeSource'
P = {
    'world_size': (900.0, 200.0, 200.0),   # cm; X is the flight direction (the projectile's rotation follows its velocity)
    'res_max_axis': 128,
    'pivot': (-0.4, 0.0, 0.0),            # unit-space pivot: the ball near the front face of the box
    'smoke_color': (0.55, 0.55, 0.55),
    'density_albedo': 0.7,
    'density_range': (0.0, 1.0),
    'dissipation_density': 4.5,           # per second: the trail is thin by the box's back face and gone ~0.8 s after impact
    'subtraction_density': 0.05,
    'turbulence_gain': (2500.0, 3000.0),  # the two curl-noise turbulence forces (shipped 900 / 1200)
    'vorticity': 4.0,                     # vorticity confinement (shipped 2): keeps the curls alive
    'src_density': 60.0,                  # sphere source density (times User.Alive)
    'src_radius': 26.0,                   # cm; > half the ball's step at 30 fps (50 cm) so the trail does not bead
    'src_velocity': (200.0, 0.0, 0.0),    # local: air dragged forward by the ball, then it slows and curls
    'core_color': (0.171, 1.6, 0.333),    # #5CFF7E in linear (0.107, 1, 0.208), times 1.6 for the glow
}

log = []
def _sys():
    return {'refPath': SYS}
def ref(emitter=None, script=None, module=None, inputs=None, stage=None):
    d = {'system': _sys()}
    if emitter: d['emitterName'] = emitter
    if script: d['scriptName'] = script
    if stage: d['simulationStage'] = {'refPath': stage}
    if module: d['moduleName'] = module
    if inputs: d['inputNameStack'] = inputs
    return d
def f(v): return {'struct': {'refPath': '/Script/Niagara.NiagaraFloat'}, 'value': {'value': float(v)}}
def i32(v): return {'struct': {'refPath': '/Script/Niagara.NiagaraInt32'}, 'value': {'value': int(v)}}
def b(v): return {'struct': {'refPath': '/Script/Niagara.NiagaraBool'}, 'value': {'value': -1 if v else 0}}
def v3(x, y, z): return {'struct': {'refPath': '/Script/CoreUObject.Vector3f'}, 'value': {'x': x, 'y': y, 'z': z}}
def v2(x, y): return {'struct': {'refPath': '/Script/CoreUObject.Vector2f'}, 'value': {'x': x, 'y': y}}
def col(r, g, bl, a=1.0): return {'struct': {'refPath': '/Script/CoreUObject.LinearColor'}, 'value': {'r': r, 'g': g, 'b': bl, 'a': a}}
def hlsl(e): return {'struct': {'refPath': '/Script/NiagaraEditor.NiagaraExt_StackInputData_HlslExpression'}, 'value': {'hlslExpression': e}}
def enum(path, name): return {'struct': {'refPath': '/Script/NiagaraEditor.NiagaraExt_StackInputData_Enum'}, 'value': {'enum': {'refPath': path}, 'enumName': name}}
def di(props): return {'struct': {'refPath': '/Script/NiagaraEditor.NiagaraExt_StackInputData_DataInterface'}, 'value': {'propertyValues': json.dumps(props)}}

async def S(tool, **kw):
    return await call_tool('NiagaraToolsets.NiagaraToolset_System.' + tool, **kw)
async def put(r, value):
    try:
        await S('SetStackInputData', stackInputRef=r, inputData=value)
    except Exception as e:
        log.append('FAILED %s %s: %s' % (r.get('moduleName'), r.get('inputNameStack'), str(e)[:200]))

def stage_path(emitter, stage_name):
    for o in unreal.ObjectIterator(unreal.NiagaraSimulationStageBase):
        p = o.get_path_name()
        if p.startswith(SYS + ':' + emitter + '.NiagaraSimulationStage') and str(o.get_editor_property('simulation_stage_name')) == stage_name:
            return p
    raise Exception('no stage ' + stage_name)

# --- emitters ------------------------------------------------------------------------------------------------
summary = await S('GetSystemSummary', system=_sys())
if SRC in [e['emitterName'] for e in summary['emitters']]:
    await S('RemoveEmitter', emitterRef=ref(SRC))
    log.append('removed ' + SRC)

# the old ball had fixed bounds of +-130 cm: the volume renderer clipped all smoke beyond them
await S('SetSystemData', system=_sys(), systemData={'propertyValues': json.dumps({'bFixedBounds': True, 'FixedBounds': {'min': {'x': -1000, 'y': -300, 'z': -300}, 'max': {'x': 1000, 'y': 300, 'z': 300}, 'isValid': True}})})

# --- user parameters -----------------------------------------------------------------------------------------
def uvar(name, value, typ):
    return {'name': name, 'type': {'classStructOrEnum': {'refPath': typ}, 'bIsStatic': False}, 'defaultValue': value}
ws = P['world_size']
await S('AddUserVariables', system=_sys(), variablesToAdd=[
    uvar('User.Alive', f(1.0), '/Script/Niagara.NiagaraFloat'),
    uvar('User.WorldSpaceSize', v3(*ws), '/Script/CoreUObject.Vector3f'),
    uvar('User.ResolutionMaxAxis', i32(P['res_max_axis']), '/Script/Niagara.NiagaraInt32'),
    uvar('User.DrawBounds', b(False), '/Script/Niagara.NiagaraBool'),
    uvar('User.FillDensity', f(P['src_density']), '/Script/Niagara.NiagaraFloat'),
    uvar('User.FillRadius', f(P['src_radius']), '/Script/Niagara.NiagaraFloat'),
])

# --- gas grid ------------------------------------------------------------------------------------------------
await put(ref(GAS, 'EmitterUpdateScript', 'Grid3D_Gas_UpdateEmitter', ['Compensate For Actor Motion']), b(True))
await put(ref(GAS, 'EmitterUpdateScript', 'Grid3D_Gas_UpdateEmitter', ['Local Pivot']), v3(*P['pivot']))
await put(ref(GAS, 'EmitterUpdateScript', 'Grid3D_Gas_MaterialControls', ['Smoke Color']), col(*P['smoke_color']))
await put(ref(GAS, 'EmitterUpdateScript', 'Grid3D_Gas_MaterialControls', ['Density Albedo']), f(P['density_albedo']))
await put(ref(GAS, 'EmitterUpdateScript', 'Grid3D_Gas_MaterialControls', ['Density Range']), v2(*P['density_range']))
# no particle source: point the (hidden) reader back at the grid emitter itself, as shipped, then switch it off
await put(ref(GAS, 'EmitterSpawnScript', 'Grid3D_Gas_InitializeParticleSource', ['Use Emitter Source']), b(True))
await put(ref(GAS, 'EmitterSpawnScript', 'Grid3D_Gas_InitializeParticleSource', ['Use Streaking']), b(False))
await put(ref(GAS, 'EmitterSpawnScript', 'Grid3D_Gas_InitializeParticleSource', ['Particle Reader']),
          di({'EmitterBinding': {'bindingMode': 'Other', 'emitterName': GAS}}))
await put(ref(GAS, 'EmitterSpawnScript', 'Grid3D_Gas_InitializeParticleSource', ['Use Emitter Source']), b(False))
sourcing = stage_path(GAS, 'Sourcing')
await S('SetModuleEnabled', moduleRef=ref(GAS, module='Grid3D_Gas_SphereSource', stage=sourcing), bEnabled=True)
await put(ref(GAS, module='Grid3D_Gas_SphereSource', stage=sourcing, inputs=['Density']), hlsl('User.FillDensity * User.Alive'))
await put(ref(GAS, module='Grid3D_Gas_SphereSource', stage=sourcing, inputs=['Temperature']), f(0.0))
await put(ref(GAS, module='Grid3D_Gas_SphereSource', stage=sourcing, inputs=['Velocity']), hlsl('float3(%.1f, %.1f, %.1f) * User.Alive' % P['src_velocity']))
presim = stage_path(GAS, 'Pre Sim Velocity')
for mod, gain in zip(('Grid3D_Turbulence001', 'Grid3D_Turbulence002'), P['turbulence_gain']):
    await put(ref(GAS, module=mod, stage=presim, inputs=['Turbulence Gain']), v3(gain, gain, gain))
await put(ref(GAS, module='Grid3D_VorticityConfinementForce', stage=presim, inputs=['VorticityMult']), f(P['vorticity']))
advect = stage_path(GAS, 'Advect Scalars')
await put(ref(GAS, module='Grid3D_Advect_FluidAttribute', stage=advect, inputs=['Dissipation Rate Density']), f(P['dissipation_density']))
await put(ref(GAS, module='Grid3D_Advect_FluidAttribute', stage=advect, inputs=['Subtraction Amount Density']), f(P['subtraction_density']))

# --- green core ----------------------------------------------------------------------------------------------
await put(ref('Core', 'EmitterUpdateScript', 'SpawnBurst_Instantaneous', ['Use Spawn Probability']), b(True))
await put(ref('Core', 'EmitterUpdateScript', 'SpawnBurst_Instantaneous', ['Spawn Probability']), hlsl('User.Alive'))
await put(ref('Core', 'ParticleSpawnScript', 'InitializeParticle', ['Color']), col(*P['core_color']))
await put(ref('Core', 'ParticleSpawnScript', 'InitializeParticle', ['AdjustHue']), b(False))
topo = await S('GetEmitterTopology', emitterRef=ref('Core'))
if not any(m['moduleName'].startswith('KillParticles') for m in topo['particleUpdateScript']['modules']):
    await S('AddModule', moduleLocationRef=ref('Core', 'ParticleUpdateScript'),
            moduleAsset={'refPath': '/Niagara/Modules/Update/Lifetime/KillParticles.KillParticles'})
    log.append('added Core KillParticles')
await put(ref('Core', 'ParticleUpdateScript', 'KillParticles', ['Kill Particles']), hlsl('User.Alive < 0.5'))

state = await S('GetSystemCompileState', system=_sys())
log.append('compile: %s errors=%s warnings=%s' % (state.get('aggregateStatus'), state.get('bHasErrors'), state.get('bHasWarnings')))
for s in state.get('scripts', []):
    if s.get('errorSummary'):
        log.append('  %s %s: %s' % (s.get('emitterName'), s.get('scriptName'), s.get('errorSummary')[:300]))
if SAVE:
    unreal.EditorAssetLibrary.save_asset('/Game/Glyphbound/FX/Reaper/NS_Reaper_PlagueBall', only_if_is_dirty=False)
    log.append('saved')
'\n'.join(log) or 'ok'
