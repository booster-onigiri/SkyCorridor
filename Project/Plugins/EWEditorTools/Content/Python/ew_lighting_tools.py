import json
import unreal
import toolset_registry
from toolset_registry.registration import Registration

def day_cycle():
    worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
    if len(worlds)!=1:
        raise RuntimeError('Start exactly one in-editor play session first.')
    actors=unreal.GameplayStatics.get_all_actors_of_class(worlds[0],unreal.EWDayCycle)
    if len(actors)!=1:
        raise RuntimeError('The procedural city is still loading.')
    return actors[0]

@unreal.uclass()
class EWLightingTools(unreal.ToolsetDefinition):
    """Preview the city's lighting at a selected hour and one of five fixed views."""

    @toolset_registry.tool_call
    @staticmethod
    def preview(hour:float,view:int,fill_lux:float,sky_floor:float,exposure_bias:float,skylight_leak:float,ambient_lux:float)->str:
        """Set a temporary lighting preview in the running PIE world.

        Args:
            hour: Game hour in 0 to 24, or -1 to resume the normal cycle.
            view: Camera view: 0 arcade, 1 plaza, 2 upper gallery, 3 side passage, 4 upward arches.
            fill_lux: Shadow-casting secondary night sky light, 0 to 80 lux.
            sky_floor: Minimum night sky intensity, 0 to 0.5.
            exposure_bias: Temporary exposure adjustment in -2 to 2 EV.
            skylight_leak: Soft ambient allowance through occluded sky, 0 to 0.25.
            ambient_lux: Weak diffuse readability baseline, 0 to 30 lux.

        Returns:
            Lighting state and whether the bounded preview was applied. No save data is changed.
        """
        actor=day_cycle()
        if not 0<=skylight_leak<=.25:raise ValueError('skylight_leak must be between 0 and 0.25')
        actor.set_editor_property('skylight_leak',skylight_leak)
        if not 0<=ambient_lux<=30:raise ValueError('ambient_lux must be between 0 and 30')
        actor.set_editor_property('ambient_lift_lux',ambient_lux)
        applied=actor.preview_lighting(hour,view,fill_lux,sky_floor,exposure_bias)
        return json.dumps({'applied':applied,'state':json.loads(actor.lighting_preview_evidence())})

    @toolset_registry.tool_call
    @staticmethod
    def inspect()->str:
        """Return the current day-cycle light intensities, shadows and exposure."""
        return day_cycle().lighting_preview_evidence()

    @toolset_registry.tool_call
    @staticmethod
    def lighting91_probe()->str:
        """Read actual sky, light and post-process components in isolated PIE, without altering time or render settings."""
        world=unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
        result={'cycle':json.loads(day_cycle().lighting_preview_evidence()),'components':[]}
        classes=[unreal.DirectionalLightComponent,unreal.SkyLightComponent,unreal.SkyAtmosphereComponent,unreal.ExponentialHeightFogComponent,unreal.PostProcessComponent]
        fields=['intensity','visible','atmosphere_sun_light','atmosphere_sun_light_index','real_time_capture','source_type','cubemap','transform_mode','bottom_radius','atmosphere_height','sky_luminance_factor','fog_density','fog_height_falloff','fog_inscattering_luminance','priority','blend_weight','unbound']
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor):
            for cls in classes:
                for component in actor.get_components_by_class(cls):
                    item={'actor':actor.get_name(),'name':component.get_name(),'class':component.get_class().get_name(),'location':str(component.get_world_location()),'rotation':str(component.get_world_rotation())}
                    for field in fields:
                        try:item[field]=str(component.get_editor_property(field))
                        except Exception:pass
                    result['components'].append(item)
            if isinstance(actor,unreal.PostProcessVolume):
                s=actor.get_editor_property('settings')
                item={'actor':actor.get_name(),'unbound':actor.get_editor_property('unbound'),'priority':actor.get_editor_property('priority')}
                for field in ['auto_exposure_method','auto_exposure_min_brightness','auto_exposure_max_brightness','auto_exposure_bias','lumen_scene_lighting_update_speed','lumen_final_gather_lighting_update_speed','override_lumen_scene_lighting_update_speed','override_lumen_final_gather_lighting_update_speed']:
                    item[field]=str(s.get_editor_property(field))
                result['components'].append(item)
        water=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.EWWaterView)
        result['water']=json.loads(water[0].water_evidence()) if water else None
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        result['camera']=str(pc.get_view_target().get_actor_location())
        return json.dumps(result)

    @toolset_registry.tool_call
    @staticmethod
    def lighting91_clock(hour:float,view:int=0,repair:bool=False)->str:
        """Exercise a normal running clock after a bounded time jump in isolated PIE. View 0 retains the camera, 1 turns the rooftop camera toward the city. Repair toggles the two auxiliary lights out of atmosphere slots for an A/B test."""
        day=day_cycle()
        if not day.preview_clock(hour):raise ValueError('Expected hour 0..24 in PIE')
        for c in day.get_components_by_class(unreal.DirectionalLightComponent):
            if c.get_name() in ['NightStreetFill','NightStreetBounce']:c.set_atmosphere_sun_light(not repair)
        world=unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
        if view==1:
            unreal.GameplayStatics.get_player_controller(world,0).set_control_rotation(unreal.Rotator(pitch=-8,yaw=130,roll=0))
        elif view!=0:raise ValueError('Expected view 0 or 1')
        return EWLightingTools.lighting91_probe()

    @toolset_registry.tool_call
    @staticmethod
    def social_menu()->str:
        """Open the city browser in PIE and inspect its real state. Does not authenticate, create a lobby or activate a microphone. Use an isolated EWDataDir."""
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one PIE session first.')
        actors=unreal.GameplayStatics.get_all_actors_of_class(worlds[0],unreal.EWSocialSession)
        if len(actors)!=1:raise RuntimeError('The city session is not ready.')
        actors[0].open_city_menu()
        return actors[0].social_evidence()

    @toolset_registry.tool_call
    @staticmethod
    def social_inspect()->str:
        """Read connection, voice, sync and persistence evidence from PIE, omitting identity IDs and credentials."""
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one PIE session first.')
        actors=unreal.GameplayStatics.get_all_actors_of_class(worlds[0],unreal.EWSocialSession)
        if len(actors)!=1:raise RuntimeError('The city session is not ready.')
        return actors[0].social_evidence()

    @toolset_registry.tool_call
    @staticmethod
    def fishing_preview(site:int)->str:
        """Move to fishing site 0 clock quay, 1 north garden or 2 south canal in PIE. Use isolated EWDataDir; normal position autosave applies. Does not grant a fish."""
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one PIE session first.')
        actors=unreal.GameplayStatics.get_all_actors_of_class(worlds[0],unreal.EWFishing)
        if len(actors)!=1:raise RuntimeError('Fishing is not ready.')
        return json.dumps({'applied':actors[0].preview_fishing(site),'state':json.loads(actors[0].fishing_evidence())})

    @toolset_registry.tool_call
    @staticmethod
    def fishing_action(action:str)->str:
        """Use real local fishing gameplay in PIE: inspect, interact, journal, display-last, resume or cancel. Fish are rolled and saved only by the normal cast/wait/reel flow."""
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one PIE session first.')
        actors=unreal.GameplayStatics.get_all_actors_of_class(worlds[0],unreal.EWFishing)
        if len(actors)!=1:raise RuntimeError('Fishing is not ready.')
        applied=True if action=='inspect' else actors[0].fishing_action(action)
        return json.dumps({'applied':applied,'state':json.loads(actors[0].fishing_evidence())})

    @toolset_registry.tool_call
    @staticmethod
    def photo_action(action:str)->str:
        """Exercise the real local photo mode in PIE: open, inspect, shoot, portrait, selfie, names, timer, left, right or close. Photos are saved under the current isolated EWDataDir/Screenshots."""
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one PIE session first.')
        actors=unreal.GameplayStatics.get_all_actors_of_class(worlds[0],unreal.EWPhotoMode)
        if len(actors)!=1:raise RuntimeError('Photo mode is not ready.')
        applied=actors[0].photo_action(action)
        return json.dumps({'applied':applied,'state':json.loads(actors[0].photo_evidence())})

    @toolset_registry.tool_call
    @staticmethod
    def night_lights(gain:float)->str:
        """Temporarily set added night fixtures to 0..3 times authored intensity in PIE; 0 provides an identical-camera comparison."""
        day=day_cycle()
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        actors=unreal.GameplayStatics.get_all_actors_of_class(worlds[0],unreal.EWNightLighting)
        if len(actors)!=1:raise RuntimeError('Night fixture actor is not ready.')
        if not actors[0].preview_night_lights(gain):raise ValueError('Expected finite gain from 0 to 3 in PIE.')
        return day.lighting_preview_evidence()

    @toolset_registry.tool_call
    @staticmethod
    def city82_preview(place:str)->str:
        """Visit cinema, sky or rail using real game travel in isolated PIE. No media starts and no microphone is opened."""
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one PIE session first.')
        instance=unreal.GameplayStatics.get_game_instance(worlds[0])
        if place=='sky':instance.visit_sky_theatre()
        elif place=='cinema':instance.visit_cinema()
        elif place=='rail':instance.visit_skyrail()
        else:raise ValueError('Expected sky, cinema or rail.')
        return json.dumps({'destination':place,'requested':True})

    @toolset_registry.tool_call
    @staticmethod
    def workshop_action(action:str,value:str)->str:
        """Use the actual local world workshop: open, inspect, egg, add, remove, move, undo, new. Value is a local catalogue filename or a chess move such as e2e4. Use isolated EWDataDir; normal autosave applies."""
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one PIE session first.')
        actors=unreal.GameplayStatics.get_all_actors_of_class(worlds[0],unreal.EWConceptRuntime)
        if len(actors)!=1:raise RuntimeError('Workshop not ready.')
        return actors[0].workshop_action(action,value)

    @toolset_registry.tool_call
    @staticmethod
    def city82_screen(sky:bool,film:bool,seat:int)->str:
        """Inspect enlarged screens in isolated PIE; optionally display the bundled film fixture and sit in a real seat. The verification editor must be launched with -nosound."""
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one PIE session first.')
        screens=sorted(unreal.GameplayStatics.get_all_actors_of_class(worlds[0],unreal.EWMediaScreen),key=lambda actor:actor.get_name())
        if len(screens)!=3:raise RuntimeError('Expected the plaza, indoor and sky screen instances.')
        # AttachWorld creates these three instances in this explicit order.
        screen=screens[2 if sky else 1]
        result=screen.preview_screen(film,seat)
        state=json.loads(result)
        if bool(state['sky_theatre'])!=sky or bool(state['cinema'])==sky:raise RuntimeError('Screen instance order changed.')
        return result

    @toolset_registry.tool_call
    @staticmethod
    def rail_preview(station:int,exterior:bool,line:int=0)->str:
        """Preview line 0 water service (0 Clock Plaza, 1 Water City), or line 1 upper service (0 Hotel, 1 Theatre, 2 Airport). Isolated PIE data only."""
        day_cycle()
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        actors=unreal.GameplayStatics.get_all_actors_of_class(worlds[0],unreal.EWSkyrail)
        actors=[a for a in actors if int(a.get_editor_property('line'))==line]
        if len(actors)!=1:raise RuntimeError('The requested train is not ready.')
        applied=actors[0].preview_station(station,exterior)
        return json.dumps({'applied':applied,'state':json.loads(actors[0].rail_evidence())})

    @toolset_registry.tool_call
    @staticmethod
    def rail88_sign(line:int,station:int,hour:float,guide:int=-1)->str:
        """Inspect a departure board, or guide 0..6, at hour 0..24 in isolated PIE. Camera and preview time only; no sound or media."""
        if line not in (0,1) or not 0<=station<(2 if line==0 else 3) or not 0<=hour<=24:
            raise ValueError('Invalid line, station or hour')
        day_cycle().preview_lighting(hour,0,40,.4,0)
        result=EWLightingTools.rail_preview(station,True,line)
        world=unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
        train=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.EWSkyrail) if int(a.get_editor_property('line'))==line)
        components=train.get_components_by_class(unreal.WidgetComponent)
        if guide<0:sign=next(c for c in components if c.get_name()=='StationSign'+str(station))
        else:
            guides=[c for c in components if c.get_name().startswith('WidgetComponent')]
            if not 0<=guide<len(guides):raise ValueError('Guide not ready or out of range')
            sign=guides[guide]
        target=sign.get_world_location()
        eye=target+sign.get_forward_vector()*300+unreal.Vector(0,0,25)
        camera=unreal.GameplayStatics.get_player_controller(world,0).get_view_target()
        camera.set_actor_location_and_rotation(eye,unreal.MathLibrary.find_look_at_rotation(eye,target),False,True)
        return result

    @toolset_registry.tool_call
    @staticmethod
    def hotel83_visit(index:int)->str:
        """Visit one of the eight upper suites, index 0..7, using normal travel in isolated PIE."""
        if not 0<=index<8:raise ValueError('Expected 0..7.')
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one PIE session first.')
        unreal.GameplayStatics.get_game_instance(worlds[0]).visit_sky_residence(index)
        return json.dumps({'requested':True,'suite':index})

    @toolset_registry.tool_call
    @staticmethod
    def hotel83_view(index:int,hour:float,view:int)->str:
        """After hotel83_visit, preview suite 0..7 at hour 0..23.99: view 0 living, 1 bed, 2 terrace, 3 entrance. Read only camera/time changes in isolated PIE."""
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one PIE session first.')
        applied=unreal.GameplayStatics.get_game_instance(worlds[0]).preview_sky_residence(index,hour,view)
        return json.dumps({'applied':applied,'suite':index,'hour':hour,'view':view})


    @toolset_registry.tool_call
    @staticmethod
    def exploration85_visit(index:int)->str:
        """Visit one of six public destinations using normal travel in isolated PIE."""
        if not 0<=index<6:raise ValueError('Expected 0..5')
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one PIE session')
        unreal.GameplayStatics.get_game_instance(worlds[0]).visit_public_place(index)
        return json.dumps({'requested':True,'place':index})

    @toolset_registry.tool_call
    @staticmethod
    def exploration85_view(index:int,hour:float,view:int)->str:
        """Inspect public place: view 0 inside, 1 outside, 2 reading gallery."""
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one PIE session')
        applied=unreal.GameplayStatics.get_game_instance(worlds[0]).preview_public_place(index,hour,view)
        return json.dumps({'applied':applied,'place':index})

    @toolset_registry.tool_call
    @staticmethod
    def cascade86_visit()->str:
        """Visit the new exterior water district in isolated PIE; no media or microphone starts."""
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one PIE session')
        unreal.GameplayStatics.get_game_instance(worlds[0]).visit_outer_water()
        return json.dumps({'requested':True})

    @toolset_registry.tool_call
    @staticmethod
    def cascade86_view(view:int,hour:float)->str:
        """Inspect exterior district: 0 gateway, 1 raised belvedere, 2 sanctuary, 3 waterfall."""
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one PIE session')
        applied=unreal.GameplayStatics.get_game_instance(worlds[0]).preview_outer_water(view,hour)
        return json.dumps({'applied':applied})

    @toolset_registry.tool_call
    @staticmethod
    def aero87_visit()->str:
        """Visit the boardable airship port in isolated PIE. Does not start media or audio."""
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one PIE session')
        unreal.GameplayStatics.get_game_instance(worlds[0]).visit_skyport()
        return json.dumps({'requested':True})

    @toolset_registry.tool_call
    @staticmethod
    def aero87_view(ship:int,view:int,hour:float)->str:
        """Inspect yacht 0/1: view 0 exterior, 1 pool deck, 2 salon, 3 bar, 4 stairs. Isolated PIE only."""
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one PIE session')
        actors=unreal.GameplayStatics.get_all_actors_of_class(worlds[0],unreal.EWAeroYacht)
        actor=next(a for a in actors if a.index==ship)
        result=actor.preview_yacht(view,hour)
        return json.dumps({'applied':result,'state':json.loads(actor.yacht_evidence())})

    @toolset_registry.tool_call
    @staticmethod
    def aero87_status()->str:
        """Read current yacht motion and evening-lamp state without moving the camera."""
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one PIE session')
        actors=unreal.GameplayStatics.get_all_actors_of_class(worlds[0],unreal.EWAeroYacht)
        return json.dumps([json.loads(a.yacht_evidence()) for a in actors])

    @toolset_registry.tool_call
    @staticmethod
    def tablet94(screen:int,action:str='status',x:float=-1,y:float=-1,name:str='')->str:
        """Inspect the plaza (0), cinema (1), or sky (2) physical media tablet in isolated PIE. Preview approach/open/close/film/day/night/far/below/behind/escape/type/status. Pointer taps route through the world widget; never opens external media."""
        if 'sky92-visual-tablet94-' not in unreal.SystemLibrary.get_command_line():raise RuntimeError('Use the isolated tablet94 verification editor')
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1 or screen not in [0,1,2]:raise RuntimeError('One PIE world and screen 0..2 required')
        screens=sorted(unreal.GameplayStatics.get_all_actors_of_class(worlds[0],unreal.EWMediaScreen),key=lambda a:a.get_name())
        if len(screens)!=3:raise RuntimeError('Expected three screen instances')
        t=screens[screen]
        if action not in ['status','approach','open','open_direct','close','film','day','night','far','below','behind','escape','type','tap','capture']:raise ValueError('Unknown tablet preview action')
        routed=t.tablet_tap(x,y) if action=='tap' else None
        if action=='capture':
            from pathlib import Path
            if not name or any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789-' for c in name):raise ValueError('Use a short evidence name')
            out=Path(unreal.Paths.project_dir()).parent/'Evidence'/('tablet94-'+name+'.png')
            if out.exists():raise RuntimeError('Preserving existing evidence')
            t.capture_tablet(str(out))
        return json.dumps({'routed':routed,'state':json.loads(t.tablet_preview(action))})

    @toolset_registry.tool_call
    @staticmethod
    def memory89(action:str,index:int=0)->str:
        """Isolated PIE memory terminal preview. Actions: status, home, map, journal, video, friends, close, scan, record, visit. No external video or account requests."""
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one isolated PIE session')
        actors=unreal.GameplayStatics.get_all_actors_of_class(worlds[0],unreal.EWTerminal)
        if len(actors)!=1:raise RuntimeError('Memory terminal not ready')
        t=actors[0]
        if action=='home':t.open();t.show_page(0)
        elif action in ['map','journal','video','friends']:t.show_page(['home','map','journal','video','friends'].index(action))
        elif action=='close':t.close()
        elif action=='scan':t.toggle_observation()
        elif action=='record':t.record_nearby()
        elif action=='visit':t.preview_at(index)
        elif action!='status':raise ValueError('Unknown preview action')
        return t.terminal_evidence()

    @toolset_registry.tool_call
    @staticmethod
    def memory89_tap(x:float,y:float)->str:
        """Click the projected handheld screen through Slate pointer routing in isolated PIE. Pixels are on its 720 x 1440 display. No external video or account requests."""
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one isolated PIE session')
        t=unreal.GameplayStatics.get_all_actors_of_class(worlds[0],unreal.EWTerminal)[0]
        return json.dumps({'routed':t.preview_tap(x,y),'state':json.loads(t.terminal_evidence())})

    @toolset_registry.tool_call
    @staticmethod
    def memory89_trace(strength:float,opacity:float,hour:float)->str:
        """Temporarily tune the trace contrast and game hour in isolated PIE; source and saves are unchanged."""
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one isolated PIE session')
        t=unreal.GameplayStatics.get_all_actors_of_class(worlds[0],unreal.EWTerminal)[0]
        return json.dumps({'applied':t.preview_trace(strength,opacity,hour)})

    @toolset_registry.tool_call
    @staticmethod
    def pool90(action:str,index:int=0,view:int=0,hour:float=12)->str:
        """Rooftop pool preview in isolated PIE: visit, view, or status. No external media or sound."""
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one isolated PIE session')
        actors=unreal.GameplayStatics.get_all_actors_of_class(worlds[0],unreal.EWWaterView)
        if len(actors)!=1:raise RuntimeError('Water view not ready')
        water=actors[0]
        if action=='visit':water.visit_pool(index)
        elif action=='view':water.preview_pool(index,view,hour)
        elif action=='optics':water.preview_optics(view)
        elif action!='status':raise ValueError('Unknown pool action')
        return water.water_evidence()

    @toolset_registry.tool_call
    @staticmethod
    def sky92(action:str,index:int=0,view:int=0,hour:float=12,name:str='phone')->str:
        """Isolated phone and water-city landmarks: visit, view, observe_app, capture, status. No external media or account actions."""
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one isolated PIE session')
        t=unreal.GameplayStatics.get_all_actors_of_class(worlds[0],unreal.EWTerminal)[0]
        applied=True
        if action=='visit':applied=t.preview_at(12+index)
        elif action=='view':applied=t.preview_landmark(index,view,hour)
        elif action=='observe_app':t.show_page(5)
        elif action=='capture':
            from pathlib import Path
            if not name.replace('-','').replace('_','').isalnum():raise ValueError('Simple evidence name required')
            out=Path(unreal.Paths.project_dir()).parent/'Evidence'/('sky92-'+name+'.png')
            if out.exists():raise RuntimeError('Preserve existing evidence')
            t.capture_screen(str(out))
        elif action!='status':raise ValueError('Unknown action')
        return json.dumps({'applied':applied,'terminal':json.loads(t.terminal_evidence())})

    @toolset_registry.tool_call
    @staticmethod
    def sky92_environment(density:float=-1,voxelize:int=-1,debug:int=-1,hzb:int=-1,raise_clouds:bool=False,height:float=-1,at_camera:bool=False)->str:
        """Inspect real cloud components in isolated PIE. Optionally preview radial extinction 0..10 or volumetric injection 0/1. Defaults are read-only."""
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one isolated PIE session')
        world=worlds[0];items=[]
        if voxelize in [0,1]:unreal.SystemLibrary.execute_console_command(world,'r.LocalFogVolume.RenderIntoVolumetricFog '+str(voxelize))
        if debug in [0,1,2]:unreal.SystemLibrary.execute_console_command(world,'r.LocalFogVolume.TileDebug '+str(debug))
        if hzb in [0,1]:unreal.SystemLibrary.execute_console_command(world,'r.LocalFogVolume.UseHZB '+str(hzb))
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor):
            for c in actor.get_components_by_class(unreal.LocalFogVolumeComponent):
                if 0<=density<=10:c.set_radial_fog_extinction(density)
                if 0<=height<=10:
                    c.set_height_fog_extinction(height);c.set_height_fog_falloff(100)
                if raise_clouds:
                    p=c.get_world_location();p.z+=5000;c.set_world_location(p,False,False)
                if at_camera:
                    pc=unreal.GameplayStatics.get_player_camera_manager(world,0)
                    p=pc.get_camera_location()+unreal.MathLibrary.get_forward_vector(pc.get_camera_rotation())*2200
                    c.set_world_location(p,False,False)
                    c.set_world_scale3d(unreal.Vector(2,2,2))
                items.append({'actor':actor.get_name(),'component':c.get_name(),'location':str(c.get_world_location()),'scale':str(c.get_world_scale()),
                    'visible':c.is_visible(),'density':c.get_editor_property('radial_fog_extinction'),'start':c.get_editor_property('fog_start_distance')})
        cv={k:unreal.SystemLibrary.get_console_variable_int_value(k) for k in ['r.LocalFogVolume','r.SupportLocalFogVolumes','r.LocalFogVolume.RenderIntoVolumetricFog','r.VolumetricFog']}
        t=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.EWTerminal)[0]
        return json.dumps({'clouds':items,'cvars':cv,'renderer':json.loads(t.sky92_fog_evidence())})

    @toolset_registry.tool_call
    @staticmethod
    def sky92_close_editor()->str:
        """Close the isolated verification editor after StopPIE. Refuses a running game or a non-sky92 verification command line."""
        if unreal.EditorLevelLibrary.get_pie_worlds(False):raise RuntimeError('Stop PIE first')
        if 'sky92-visual-' not in unreal.SystemLibrary.get_command_line():raise RuntimeError('Not the isolated sky92 verification editor')
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        unreal.SystemLibrary.execute_console_command(world,'QUIT_EDITOR')
        return 'Requested isolated editor shutdown'

    @toolset_registry.tool_call
    @staticmethod
    def quality93_profile(name:str)->str:
        """Capture 600 real engine frames with GPU timings in an isolated quality93 PIE; no user save or media actions."""
        import re
        if 'sky92-visual-quality93-' not in unreal.SystemLibrary.get_command_line():raise RuntimeError('Use the isolated quality93 editor')
        if not re.fullmatch('[a-z0-9-]{1,48}',name):raise ValueError('Simple evidence name required')
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one PIE')
        for cmd in ['r.GPUCsvStatsEnabled 1','csv.CompressionMode 0','CsvProfile STARTFILE=quality93-'+name+'.csv','CsvProfile FRAMES=600']:
            unreal.SystemLibrary.execute_console_command(worlds[0],cmd)
        return json.dumps({'requested':True,'frames':600,'name':name,'profiling_directory':unreal.Paths.profiling_dir()})

    @toolset_registry.tool_call
    @staticmethod
    def quality93_detail(step:float=0)->str:
        """Move the existing inspection camera forward at most three metres for a detail review. Does not move the character."""
        if 'sky92-visual-quality93-' not in unreal.SystemLibrary.get_command_line():raise RuntimeError('Use the isolated quality93 editor')
        if abs(step)>300:raise ValueError('At most 300 cm')
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start PIE')
        camera=unreal.GameplayStatics.get_player_controller(worlds[0],0).get_view_target()
        if not isinstance(camera,unreal.CameraActor):raise RuntimeError('Select an inspection camera first')
        camera.set_actor_location(camera.get_actor_location()+camera.get_actor_forward_vector()*step,False,True)
        return json.dumps({'location':str(camera.get_actor_location()),'rotation':str(camera.get_actor_rotation())})

    @toolset_registry.tool_call
    @staticmethod
    def quality93_nanite(enabled:int=-1,pool_mb:int=0)->str:
        """Inspect Nanite and optionally compare its rendering in the isolated graphics audit only."""
        if 'sky92-visual-quality93-' not in unreal.SystemLibrary.get_command_line():raise RuntimeError('Use isolated quality93 editor')
        if enabled not in [-1,0,1] or pool_mb not in [0,512,1024]:raise ValueError('Bounded diagnostic settings only')
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds)!=1:raise RuntimeError('Start exactly one PIE')
        world=worlds[0]
        if enabled>=0:unreal.SystemLibrary.execute_console_command(world,'r.Nanite '+str(enabled))
        if pool_mb:unreal.SystemLibrary.execute_console_command(world,'r.Nanite.Streaming.StreamingPoolSize '+str(pool_mb))
        keys=['r.Nanite','r.Nanite.ProxyRenderMode','r.Nanite.Streaming.StreamingPoolSize','r.Nanite.MaxVisibleClusters','r.Nanite.MaxCandidateClusters','r.Nanite.MaxNodes']
        data={'cvars':{key:unreal.SystemLibrary.get_console_variable_int_value(key) for key in keys},'components':[]}
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor):
            for c in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
                mesh=c.get_editor_property('static_mesh')
                if not mesh or mesh.get_name()!='SM_Explore85Furniture2':continue
                row={'actor':actor.get_name(),'instances':c.get_instance_count(),'nanite':str(mesh.get_editor_property('nanite_settings'))}
                for key in ['disallow_nanite','forced_lod_model','visible','hidden_in_game']:
                    try:row[key]=str(c.get_editor_property(key))
                    except Exception as exc:row[key]=str(exc)
                data['components'].append(row)
        return json.dumps(data)

registration=Registration([EWLightingTools])
