# Owner's spec: elemental build-up and status effects (2026-11-12)

> Verbatim from the owner (Spanish), pasted into the architect chat. English summary: GDD v0.22, section 6 "Elemental build-up".
> The architect read the code first, listed the conflicts with the existing systems, and asked two questions; the owner's answers:
> 1. The existing small effects on every elemental hit (fire burn, the lightning 40% slow already named "Shock", the ice enchant's chill): **remove them all**. The bar's status effect is the only elemental effect; the ice floor keeps its zone slow.
> 2. The elemental sword and the ember staff: **both build up** their element.

---

Implementación del sistema de efectos de estado elementales

Necesitamos incorporar un sistema de acumulación elemental (Elemental Build-Up) y efectos de estado para Fuego, Rayo y Hielo al juego existente.

IMPORTANTE: El proyecto ya cuenta con hechizos, armas, encantamientos elementales, enemigos, daño, cooldowns, maná, QTE y perks funcionando. Antes de implementar, revisar la arquitectura actual y reutilizar los sistemas existentes. No reconstruir ni reemplazar mecánicas que ya funcionan correctamente.

El objetivo es implementar primero el funcionamiento de los efectos de estado y dejar los valores numéricos configurables para balancearlos posteriormente.

## 1. Sistema general de acumulación elemental
Cada enemigo debe gestionar independientemente tres acumuladores: `FireBuildUp`, `LightningBuildUp`, `IceBuildUp`.
Cada acumulador aumenta cuando el enemigo recibe un impacto válido de su elemento correspondiente. Cuando una acumulación alcanza el umbral configurado, se activa su efecto de estado.
Reglas generales:
- Cada elemento posee su propia acumulación, umbral, duración, inmunidad temporal y velocidad de decaimiento.
- Si transcurre un período configurable sin recibir acumulación de un elemento, su barra comienza a disminuir progresivamente.
- Al activarse un efecto, la acumulación correspondiente vuelve a cero.
- Mientras el efecto esté activo, no se acumula nuevamente ese mismo elemento.
- Después de finalizar, puede existir un período configurable de inmunidad temporal antes de permitir una nueva acumulación.
- Los tres elementos deben funcionar independientemente.
- Los efectos de daño y control deben ser independientes de la velocidad de fotogramas.
- Evitar aplicaciones duplicadas de acumulación por un único impacto o evento de daño.

No es necesario implementar todavía reacciones especiales entre diferentes elementos.

## 2. Configuración individual por enemigo
No utilizar multiplicadores globales de resistencia por categoría de enemigo, como un multiplicador universal para élites o bosses.
Cada enemigo debe poder definir individualmente:
- Resistencia o vulnerabilidad al daño de Fuego, Rayo y Hielo.
- Resistencia o vulnerabilidad a la acumulación de cada elemento.
- Umbral de acumulación elemental.
- Duración efectiva de cada estado.
- Inmunidad temporal después de un estado.
- Si es inmune a un estado específico.
- Qué reacción realiza cuando se activa cada efecto.

Por ejemplo, una valquiria podría resistir mucho el fuego y ser vulnerable al rayo, mientras que otro boss podría presentar exactamente lo contrario.
Igualmente, un enemigo puede recibir daño eléctrico sin ser susceptible a una interrupción completa.
Mantener independientes las resistencias al daño y las resistencias a la acumulación, para poder balancearlas por separado.
Utilizar valores por defecto únicamente como respaldo para enemigos sin configuración específica, no como sustituto de sus características individuales.

## 3. Estado de Fuego — Ignite
Identidad: daño sostenido.
Al alcanzar el umbral de acumulación de fuego, el enemigo entra en estado `Ignite`.
Comportamiento:
- Aplica daño periódico durante una duración configurable.
- El daño se calcula mediante intervalos de tiempo definidos, no por frame.
- No debe activar nuevos estados Ignite mientras el actual siga activo.
- Al finalizar, se aplica la inmunidad temporal configurada para ese enemigo.
- Cada enemigo puede tener diferentes resistencias, duración y daño de quemadura.

Fuentes de acumulación: Fireball, Fire Wall, Flamethrower, armas encantadas con Fuego.
Cada fuente debe tener su propio valor de acumulación.
Por ejemplo, Flamethrower debería poder acumular más fuego mediante exposición continua que un único impacto de Fireball, pero los valores exactos se determinarán posteriormente.
Compatibilidad: revisar el efecto de quemadura que ya posee Fireball y reutilizarlo o integrarlo con Ignite para evitar dos sistemas de DOT que se dupliquen accidentalmente. No cambiar silenciosamente su comportamiento actual; documentar cualquier ajuste necesario.

## 4. Estado de Rayo — Shock
Identidad: interrupción y control inmediato.
Al alcanzar el umbral de acumulación eléctrica, el enemigo entra en estado `Shock`.
Comportamiento:
- Produce una descarga eléctrica.
- Puede interrumpir el ataque o habilidad que el enemigo esté ejecutando.
- Puede provocar una breve reacción de aturdimiento o desestabilización.
- La reacción debe ser configurable individualmente por enemigo.
- Determinados enemigos pueden resistir el aturdimiento, pero recibir una interrupción menor.
- Otros pueden ser completamente inmunes al efecto de control.
- Al finalizar, debe respetarse la inmunidad temporal configurada para evitar interrupciones continuas.

No establecer reglas universales que obliguen a todos los bosses a reaccionar de la misma manera.
Fuentes de acumulación: Lightning Bolt, Lightning Rune, Static Orb, armas encantadas con Rayo.
Cada fuente posee su propia acumulación configurable.
En Lightning Rune, el radio de detección que activa la trampa y el radio de su explosión son diferentes. La acumulación se aplica a los enemigos alcanzados por la explosión, respetando el comportamiento actual de `Expanded Sigil`.

## 5. Estado de Hielo — Frost
Identidad: ralentización y control prolongado.
Al alcanzar el umbral de acumulación de hielo, el enemigo entra en estado `Frost`.
Comportamiento:
- Puede congelar temporalmente al enemigo o reducir considerablemente su velocidad.
- La reacción se configura individualmente por enemigo.
- Algunos enemigos podrán quedar inmovilizados.
- Otros sufrirán una ralentización fuerte.
- Determinados enemigos podrán resistir el congelamiento o ser inmunes.
- Al finalizar, se respeta la inmunidad temporal correspondiente.

Fuentes de acumulación: Ice Floor, armas encantadas con Hielo.
Por ahora, Ice Floor será el único hechizo que aplique acumulación de hielo.
Ice Wall e Ice Shield no deben aplicar acumulación elemental.
La acumulación generada por Ice Floor debe ser relativamente baja y configurable.
La intención es que Ice Floor pueda acumular hielo gradualmente mientras los enemigos permanecen dentro de su zona, pero que el jugador pueda completar esa acumulación más rápidamente utilizando un arma encantada con Hielo.
Compatibilidad con Ice Floor:
- La ralentización base actual es del 50%.
- `Deep Freeze` la aumenta a 55%, 60% y 65% según su nivel.
- El límite máximo configurado es del 80%.
- Ya existe interacción con el QTE.

No reemplazar ni modificar esos comportamientos.
La ralentización adicional de Frost no debe sumarse de manera descontrolada a la ralentización de Ice Floor. Utilizar una regla consistente, como aplicar el efecto de ralentización más fuerte, respetando los límites correspondientes.

## 6. Acumulación específica de cada hechizo
No establecer un valor fijo de acumulación para todos los hechizos de un mismo elemento.
Cada hechizo debe poder configurar individualmente:
- Elemento aplicado.
- Acumulación por impacto o tick válido.
- Intervalo entre aplicaciones de acumulación, cuando corresponda.
- Si puede aplicar acumulación o no.

Los hechizos continuos deben utilizar intervalos controlados para impedir que la acumulación dependa de los FPS.
En particular, Flamethrower ya utiliza intervalos de daño de 0,3 segundos. Procurar reutilizar esos eventos para aplicar acumulación, evitando introducir procesos de daño adicionales innecesarios.
Mantener intactas sus demás mecánicas existentes, incluidos el consumo inicial de maná, el consumo por segundo, el QTE, la canalización mientras se mantiene la tecla y la interrupción por daño fuerte.

## 7. Acumulación mediante Weapon Enchant
Las armas encantadas deben aplicar acumulación del elemento seleccionado.
Elementos disponibles: Fire, Lightning, Ice.
Reglas:
- Las armas sin encantamiento no aplican acumulación elemental, salvo que alguna tenga esa propiedad explícitamente configurada.
- Cada impacto válido de un arma encantada puede contribuir a la acumulación.
- El valor de acumulación debe poder configurarse por arma y elemento.
- La acumulación debe respetar las resistencias del enemigo.
- Los distintos tipos de armas pueden tener valores diferentes.
- Evitar acumulación excesiva causada por múltiples impactos, especialmente en armas como la escopeta.

No modificar el daño o funcionamiento de las armas si no es necesario para integrar este sistema.

## 8. Bonificación por Headshots y puntos débiles
Los impactos realizados con armas encantadas sobre la cabeza o sobre un punto débil del enemigo deben conceder una bonificación configurable de acumulación elemental.
Reglas:
- Un headshot válido obtiene la bonificación.
- Un impacto sobre un punto débil definido por el enemigo obtiene la misma bonificación.
- Los puntos débiles no tienen que ser necesariamente la cabeza.
- Si la cabeza también está marcada como punto débil, la bonificación no debe aplicarse dos veces.
- El modificador debe poder configurarse y balancearse posteriormente.
- La bonificación afecta a la acumulación elemental; no modificar automáticamente el multiplicador de daño crítico o de headshot existente.

Utilizar el sistema actual de hitboxes o detección de puntos débiles si ya existe. No crear uno paralelo innecesariamente.

## 9. Configuración mediante datos
Mantener la implementación compatible con la arquitectura actual del proyecto.
Aprovechar `Spells.json`, `Game.json` y las configuraciones existentes de enemigos y armas, cuando corresponda.
Los parámetros deberían ser editables sin modificar la lógica del sistema.
Incluir, según corresponda: `BuildUpAmount`, `BuildUpThreshold`, `BuildUpDecayRate`, `DecayDelay`, `StatusDuration`, `StatusImmunityDuration`, `StatusEffectType`, `StatusDamage`, `StatusTickInterval`, `StatusSlowAmount`, `StatusControlReaction`, `ElementalResistance`, `HeadshotBuildUpBonus`, `WeakPointBuildUpBonus`.
Los nombres son orientativos. Adaptarlos a las convenciones existentes del proyecto.
Si un parámetro ya existe con otro nombre, reutilizarlo.
Los valores iniciales pueden ser provisionales para comprobar que los tres estados funcionan, pero deben quedar claramente identificados como pendientes de balanceo.

## 10. Efectos visuales
Por ahora, implementar únicamente los eventos y estados necesarios para que el sistema pueda comunicarse con los efectos visuales posteriores.
La representación visual se decidirá más adelante y podrá utilizar Niagara, shaders o efectos particulares por enemigo.
No crear todavía un indicador visual universal obligatorio.
Cada enemigo deberá poder responder visualmente a: acumulación elemental, activación del estado, estado activo, finalización del estado.
La ausencia de efectos visuales definitivos no debe impedir que funcione el sistema.

## 11. Compatibilidad y rendimiento
Este sistema debe funcionar correctamente con múltiples enemigos simultáneamente, considerando que el juego tendrá hordas.
Priorizar:
- Reutilización de los eventos de daño existentes.
- Ausencia de comprobaciones costosas por frame cuando puedan utilizarse eventos o intervalos.
- Gestión independiente de los estados de cada enemigo.
- Evitar aplicaciones duplicadas durante explosiones, proyectiles penetrantes y ataques continuos.
- Limpieza de estados y procesos temporales cuando un enemigo muere o es destruido.
- Respeto de las mecánicas actuales de daño, resistencias, cooldowns, QTE y perks.
- Separación entre lógica de combate y representación visual.

No implementar todavía nuevas sinergias elementales, perks adicionales ni mecánicas fuera de este alcance.

## 12. Pruebas de funcionamiento
Antes de considerar terminada la implementación, comprobar:
1. Un hechizo de fuego acumula Ignite y activa el DOT al alcanzar su umbral.
2. Lightning Bolt puede acumular Shock sobre varios enemigos atravesados sin duplicar impactos.
3. Ice Floor acumula Frost gradualmente mientras el enemigo permanece en el área.
4. Un arma encantada puede completar una acumulación iniciada por un hechizo.
5. Un headshot o impacto sobre un punto débil aplica la bonificación correspondiente una sola vez.
6. Dos enemigos diferentes pueden tener resistencias y umbrales distintos para el mismo elemento.
7. Un enemigo puede ser inmune a Shock sin volverse inmune al daño eléctrico.
8. Las inmunidades temporales evitan la aplicación continua del mismo estado.
9. La acumulación disminuye correctamente cuando deja de aplicarse el elemento.
10. Los efectos funcionan con varios enemigos simultáneamente sin errores ni degradaciones evidentes.
11. Ice Floor conserva sus valores de ralentización, perk Deep Freeze y QTE.
12. Flamethrower conserva sus intervalos, consumo de maná y condiciones de interrupción.
13. El nuevo Ignite no duplica accidentalmente la quemadura existente de Fireball.

## 13. Resultado esperado
Implementar un sistema elemental modular, reutilizable y configurable que permita:
- Acumular efectos elementales mediante hechizos y armas encantadas.
- Aplicar Ignite, Shock y Frost.
- Configurar resistencias y respuestas individualmente por enemigo.
- Recompensar headshots y ataques a puntos débiles.
- Ajustar posteriormente todos los valores durante el balanceo.
- Añadir sinergias y nuevos perks en una etapa posterior sin reconstruir el sistema.

Prioridad principal: conservar la estabilidad del juego existente, implementar correctamente las tres mecánicas y evitar complejidad innecesaria durante el Game Jam.
Antes de realizar cambios, revisar las clases, archivos y sistemas actuales. Si alguna mecánica existente entra en conflicto con esta especificación, identificarla y explicar la solución propuesta. Al finalizar, resumir qué archivos se modificaron, qué quedó configurable y qué pruebas se realizaron.
