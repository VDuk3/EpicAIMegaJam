# Registro de assets 3D — 2026 Epic AI MegaJam

## Contexto

Esta computadora está encargada de producir assets 3D. Mantener en este registro la trazabilidad del trabajo realizado en ella y actualizarlo al crear, modificar o entregar assets.

## Reglas relevantes

- El período oficial es del 12 de noviembre de 2026 a las 00:00 ET al 15 de noviembre de 2026 a las 20:59 ET (01:00 del 12 a 21:59 del 15, hora de Caracas).
- Los assets originales del juego deben crearse durante el período del concurso. Las herramientas, configuraciones de agentes y MCP, bibliotecas de prompts y scripts pueden prepararse antes; Epic puede solicitar materiales o historial de versiones.
- Se permiten assets de terceros con derechos adecuados; deben declararse al enviar el juego y no serán evaluados. El contenido generado con IA no se considera asset de terceros según las reglas oficiales, aunque debe comprobarse que se cuenta con los derechos necesarios.
- El contenido de la entrega, incluido el texto del juego, y la información de inscripción deben estar en inglés. El contenido debe ser apto para mayores de 13 años. Las reglas restringen las marcas y logotipos.
- El registro de desarrollo puede servir para Glass Box. Registrar las herramientas utilizadas puede ser relevante para premios asociados; en particular, el texto del evento indica declarar Rodin para optar al fondo de Hyper3D.
- La inscripción final debe enviarla una persona, no un agente automatizado.

## Registro de producción

| Fecha y hora | Asset | Acción y estado | Origen, herramienta y versión | Licencia / derechos | Archivos y entrega | Evidencia y notas |
|---|---|---|---|---|---|---|
| 2026-11-12 | — | Registro inicial creado; en ese momento aún no se había producido ni entregado ningún asset en esta sesión. | — | — | — | Reglas oficiales y texto del evento proporcionados por el usuario. |
| 2026-11-12 | Pistola cyberpunk para primera persona | Generación, comparación y desarrollo de prototipos en curso; **no entregado ni validado en Unreal**. Se trabajó en silueta, acabado negro y blanco plateado, luces elementales, gatillo, cartucho, panel y mira. | Tripo CLI 0.11.2: H3.1 tarea `bd2a20a1-3a02-42d9-ab72-178a2eaaf53d` (30 créditos API) y P2 tarea `50e44d0b-5c36-48f2-a7a9-87d05b466f45` (110 créditos API); Meshy imagen a 3D tarea `01a1d080-cccd-775f-adb9-8e1d2625e62b` (15 créditos); edición e inspección con Blender 5.2.3 LTS y 5.3.0. | Referencia visual aportada por el usuario; incluye marcas y una referencia secundaria con marca de agua. Derechos y limpieza completa de inscripciones pendientes de verificar; no se afirma autorización de reutilización de marcas. | Originales y versiones en `C:\Epic AI GameJam\Documentation\AI3D_Weapons_Jam\`; copias actuales `work\tripo_pistol_B\pistol_optica_integrada_verde_v1.blend` y `pistol_optica_integrada_morado_v1.blend`. | Trazabilidad de tareas, hashes, decisiones, ensayos, rechazos y limitaciones: `Documentation\AI3D_Weapons_Jam\reports\01_generation_manifest.md`; renders y auditorías en `audit\`; handoff para Cursor en `Documentation\AI3D_Weapons_Jam\HANDOFF_CURSOR_PISTOLA_CYBERPUNK_2026-11-12.md`. El proyecto UE está en la computadora principal; validación de materiales, animación, POV y rendimiento pendiente. Rodin pausado por indicación del usuario. |

## Fuentes

- Reglas oficiales: `C:\Users\Romelt\Downloads\Documents\2026_Epic_AI_MegaJam_Official_Rules_EN.pdf` (actualizadas el 5 de noviembre de 2026).
- Texto del evento: `C:\Users\Romelt\.codex\attachments\431104db-98cb-4136-aa29-6e6d296f0626\Texto pegado.txt`.

| 2026-11-12 | Escopeta cyberpunk | Dos candidatos generados y auditados preliminarmente; NO_WINNER, no entregados ni validados en Unreal. | Referencia 2 del usuario; Tripo P2 (110 créditos), Meshy t2.5 (15 créditos), Blender 5.3.0. | Derechos de referencia y marcas pendientes. | Documentation/AI3D_Shotgun_Jam/raw/ y audit/ | IDs, SHA-256, limitaciones y comparación: Documentation/AI3D_Shotgun_Jam/reports/comparacion_inicial.md. Rodin pendiente por saldo y autorización. |

| 2026-11-12 | Escopeta blanca Meshy | Seleccionada por el usuario como base. Creada copia Blender y tres estudios reversibles de gatillo; rechazados como solución terminada por integración visual deficiente. | Blender 5.3.0; original Meshy previamente registrado. | Derechos de referencia pendientes. | Documentation/AI3D_Shotgun_Jam/work/meshy_white/ | Documentation/AI3D_Shotgun_Jam/reports/desarrollo_blanca_01.md; sin entrega ni validación UE. |

| 2026-11-12 | Escopeta blanca | Ensayos v4-v6: guardamonte separado, puente de culata rechazado y prueba de gatillo; intersecciones detectadas, trabajo no terminado. | Blender 5.3.0; copia Meshy. | Derechos de referencia pendientes. | Documentation/AI3D_Shotgun_Jam/work/meshy_white/ | Informe desarrollo_blanca_01.md y audit/gatillo_v6_bvh.json. No entregada ni validada en UE. |

| 2026-11-12 | Escopeta blanca | Reconstrucción local de alojamiento, guardamonte y gatillo independiente en copia v9; prueba BVH limitada de 12 fotogramas sin cruces con cuerpo ni guardamonte. Sigue en desarrollo. | Blender 5.3.0; copia del candidato Meshy. | Derechos de referencia pendientes. | Documentation/AI3D_Shotgun_Jam/work/meshy_white/escopeta_blanca_alojamiento_v9.blend | Informe desarrollo_blanca_01.md; sin validación UE ni entrega. |

| 2026-11-12 | Escopeta blanca | Unión superior provisional de culata y prueba reversible de rugosidad; inspección de nodos y vistas, sin dar por terminado el asset. | Blender 5.3.0; Meshy. | Derechos de referencia pendientes. | Documentation/AI3D_Shotgun_Jam/work/meshy_white/escopeta_blanca_material_v11.blend | Informe desarrollo_blanca_01.md; sin validación UE ni entrega. |

| 2026-11-12 | Escopeta blanca | Estudios de entrada inferior v12-v15; v12 y v13 rechazados, v15 con corte local y normales transferidas en evaluación. | Blender 5.3.0; copia de Meshy. | Derechos de referencia pendientes. | Documentation/AI3D_Shotgun_Jam/work/meshy_white/escopeta_blanca_estudio_carga_v15.blend | Informe desarrollo_blanca_01.md; recarga y sombreado por verificar, sin validación UE ni entrega. |

| 2026-11-12 | Escopeta blanca | Ensayo de inserción de un cartucho y reemplazo de gatillo tubular; pruebas superficiales limitadas y renders, sin terminar. | Blender 5.3.0; base Meshy. | Derechos de referencia pendientes. | Documentation/AI3D_Shotgun_Jam/work/meshy_white/escopeta_blanca_gatillo_v17.blend | Informe desarrollo_blanca_01.md; sin validación UE ni entrega. |

| 2026-11-12 | Escopeta blanca | Inspección posterior y reconstrucción provisional de boca de una abertura; mira aparentemente obstruida queda pendiente. | Blender 5.3.0; base Meshy. | Derechos de referencia pendientes. | Documentation/AI3D_Shotgun_Jam/work/meshy_white/escopeta_blanca_boca_v19.blend | Informe desarrollo_blanca_01.md; sin validación UE ni entrega. |

| 2026-11-12 | Escopeta blanca | Investigación de mira: abertura confirmada con cámara alineada; ensayos v20/v21 rechazados por pérdida de detalles o superposición. | Blender 5.3.0; base Meshy. | Derechos de referencia pendientes. | Documentation/AI3D_Shotgun_Jam/work/meshy_white/ | Informe desarrollo_blanca_01.md; base de trabajo v19, sin validación UE ni entrega. |

| 2026-11-12 | Escopeta blanca | Retícula y cristal provisionales con prueba visual e inspección superficial; no implementan gameplay ni óptica final. | Blender 5.3.0; base Meshy. | Derechos de referencia pendientes. | Documentation/AI3D_Shotgun_Jam/work/meshy_white/escopeta_blanca_reticula_v23.blend | Informe desarrollo_blanca_01.md; sin validación UE ni entrega. |

| 2026-11-12 | Escopeta blanca | Ajuste provisional de cristal y retícula dentro de mira; revestimientos v24-v26 descartados por deterioro. Cristal solapa carcasa intencionadamente y requiere comprobación. | Blender 5.3.0; base Meshy. | Derechos de referencia pendientes. | Documentation/AI3D_Shotgun_Jam/work/meshy_white/escopeta_blanca_reticula_v28.blend | Informe desarrollo_blanca_01.md; sin validación UE ni entrega. |

| 2026-11-12 | Escopeta blanca | Separadas escenas de reposo y prueba; grupo de material elemental con color y llenado ensayados en superficies aparte. | Blender 5.3.0; base Meshy. | Derechos de referencia pendientes. | Documentation/AI3D_Shotgun_Jam/work/meshy_white/escopeta_blanca_material_control_v30.blend | Informe desarrollo_blanca_01.md; no integrado ni validado en UE, sin entrega. |

| 2026-11-12 | Escopeta blanca | Guardamonte afinado, fragmento residual retirado y gatillo suavizado; ensayos limitados de gatillo y cartucho. | Blender 5.3.0; base Meshy. | Derechos de referencia pendientes. | Documentation/AI3D_Shotgun_Jam/work/meshy_white/escopeta_blanca_gatillo_v33.blend | Informe desarrollo_blanca_01.md; sin validación UE ni entrega. |

| 2026-11-12 | Escopeta blanca | Maqueta ampliada de inserción individual y placa separada por escena; estados y cruces superficiales ensayados. | Blender 5.3.0; base Meshy. | Derechos de referencia pendientes. | Documentation/AI3D_Shotgun_Jam/work/meshy_white/escopeta_blanca_recarga_v35.blend | Informe desarrollo_blanca_01.md; sin manos, gameplay ni validación UE; no entregada. |

| 2026-11-12 | Escopeta blanca | Auditoría de conversión estática; cristal corregido para Alpha BLEND y mapa ORM de copia con rugosidad mínima, verificados al reimportar en Blender. | Blender 5.3.0; base Meshy. | Derechos de referencia pendientes. | Documentation/AI3D_Shotgun_Jam/work/meshy_white/escopeta_blanca_mapas_v38.blend | Informe desarrollo_blanca_01.md; GLB estático de auditoría sin animación, sin validación UE ni entrega. |

| 2026-11-12 | Escopeta blanca | Ensayos de conversión de gatillo y recarga; v39 añade escala escalonada para representar ocultación en GLB de prueba. | Blender 5.3.0; base Meshy. | Derechos de referencia pendientes. | Documentation/AI3D_Shotgun_Jam/work/meshy_white/escopeta_blanca_recarga_v39.blend | Informe desarrollo_blanca_01.md; escala cero provisional, sin validación UE ni entrega. |

| 2026-11-12 | Escopeta blanca | Acciones separadas por escena: gatillo fijo durante recarga, animado en ensayo de disparo. | Blender 5.3.0; base Meshy. | Derechos de referencia pendientes. | Documentation/AI3D_Shotgun_Jam/work/meshy_white/escopeta_blanca_acciones_v40.blend | Informe desarrollo_blanca_01.md; sin validación UE ni entrega. |

| 2026-11-12 | Escopeta blanca | Escena de ensayo de siete inserciones individuales; se revisaron ciclos y superficies y se hizo conversión de auditoría. | Blender 5.3.0; base Meshy. | Derechos de referencia pendientes. | Documentation/AI3D_Shotgun_Jam/work/meshy_white/escopeta_blanca_siete_prueba_v41.blend | Informe desarrollo_blanca_01.md; sin manos, contador ni validación UE; no entregada. |

| 2026-11-12 | Escopeta blanca | Limpieza localizada de marcas tenues en copia del Base Color; comparación de ambas caras y conversión estática auditada. | Blender 5.3.0; base Meshy. | Derechos de referencia y revisión completa de marcas pendientes. | Documentation/AI3D_Shotgun_Jam/work/meshy_white/escopeta_blanca_limpieza_v42.blend | Informe desarrollo_blanca_01.md; rastros y deformaciones pendientes, sin validación UE ni entrega. |

| 2026-11-12 | Escopeta blanca | Cuatro ensayos locales de ranuras descartados por panel superpuesto o pinchos; base v42 preservada. | Blender 5.3.0; base Meshy. | Derechos de referencia pendientes. | Documentation/AI3D_Shotgun_Jam/work/meshy_white/ | Informe desarrollo_blanca_01.md; ranuras aún defectuosas, sin entrega ni validación UE. |
