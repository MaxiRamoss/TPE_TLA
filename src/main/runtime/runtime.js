/*
 * Vial runtime (spec-v3 §4 and §5).
 *
 * The compiler inserts this file, unchanged, into every HTML file it emits,
 * after the "vial-config" JSON and the generated VIAL_LOGIC. It has two
 * independent parts:
 *
 *   1. The ENGINE: the simulation model of §4, as plain functions with no DOM.
 *      It only reads the config (§5.2) and calls the generated logic (§5.3).
 *   2. The VIEWER: it reads "#vial-config" and VIAL_LOGIC, creates a
 *      simulation and draws it on the page (§5.4).
 *
 * From Node, require() returns the engine and the viewer does not run.
 */

/* ========================================================================== */
/* 1. ENGINE                                                                  */
/* ========================================================================== */

var VialEngine = (function () {
	'use strict';

	/** One cell (7.5 m) in accumulator units, which are 1/3600 m: 7.5 · 3600. */
	const CELL = 27000;

	/* ---------------------------------------------------------------------- */
	/* Formatting (§3.8): a line-by-line port of formatValue (Value.c).       */
	/* ---------------------------------------------------------------------- */

	/**
	 * Formats a duration as "1 h 2 min 5 s", omitting the zeros ("0 s" if every
	 * part is zero). A negative duration is prefixed with "-".
	 */
	function formatDuration(seconds) {
		const total = Math.abs(seconds);
		const hours = Math.floor(total / 3600);
		const minutes = Math.floor((total % 3600) / 60);
		const remainder = total % 60;
		let text = seconds < 0 ? '-' : '';
		if (0 < hours) {
			text += hours + ' h';
		}
		if (0 < minutes) {
			text += (0 < hours ? ' ' : '') + minutes + ' min';
		}
		if (0 < remainder || total === 0) {
			text += (0 < hours || 0 < minutes ? ' ' : '') + remainder + ' s';
		}
		return text;
	}

	/**
	 * Formats a speed (in m/h) in km/h: as an integer if it is exact, or else
	 * with one decimal, rounded half away from zero. A negative speed is
	 * prefixed with "-".
	 */
	function formatSpeed(metersPerHour) {
		const sign = metersPerHour < 0 ? '-' : '';
		const magnitude = Math.abs(metersPerHour);
		if (magnitude % 1000 === 0) {
			return sign + (magnitude / 1000) + ' km/h';
		}
		const tenths = Math.floor((magnitude + 50) / 100);
		return sign + Math.floor(tenths / 10) + '.' + (tenths % 10) + ' km/h';
	}

	/**
	 * Formats a value of a Vial type (its name, as in valueTypeName) for its
	 * interpolation in a string. An entity formats as its label or, if it has
	 * none, as its identifier; "nameOf(type, id)" resolves it.
	 */
	function formatValue(type, value, nameOf) {
		switch (type) {
			case 'boolean': return value ? 'true' : 'false';
			case 'distance': return value + ' m';
			case 'duration': return formatDuration(value);
			case 'integer': return String(value);
			case 'speed': return formatSpeed(value);
			case 'string': return value;
			case 'error': return '<error>';
			default: return nameOf ? nameOf(type, value) : value;
		}
	}

	/* ---------------------------------------------------------------------- */
	/* Simulation                                                             */
	/* ---------------------------------------------------------------------- */

	/**
	 * Maps the elements of an array by their "id". The maps by id have no
	 * prototype, so ids such as "constructor" or "toString", which are valid in
	 * Vial, find nothing that was not declared.
	 */
	function indexById(elements) {
		const map = Object.create(null);
		elements.forEach(function (element, k) { map[element.id] = k; });
		return map;
	}

	/**
	 * Creates a simulation from a "vial-config" object (§5.2) and VIAL_LOGIC
	 * (§5.3). Each call to step() simulates one tick of 1 s (§4.4).
	 */
	function createSimulation(config, logic) {
		const duration = config.simulation.duration;
		const intersections = config.intersections || [];
		const roads = config.roads || [];
		const lights = config.lights || [];
		const routes = config.routes || [];
		const flows = config.flows || [];
		const metrics = config.metrics || [];
		const policies = (logic && logic.policies) || {};
		const metricFunctions = (logic && logic.metrics) || [];

		/* Lookup tables: entity class → (id → index), for name() and the queries. */
		const entities = {
			intersection: intersections,
			road: roads,
			light: lights,
			route: routes,
			flow: flows,
			metric: metrics
		};
		const indexes = Object.create(null);
		Object.keys(entities).forEach(function (entityClass) {
			indexes[entityClass] = indexById(entities[entityClass]);
		});
		const roadIndex = indexes.road;

		/* The light that controls the end of each road (at most one, V13), or -1. */
		const lightAtIntersection = Object.create(null);
		lights.forEach(function (light, l) { lightAtIntersection[light.at] = l; });
		const lightOfRoad = roads.map(function (road) {
			return road.to in lightAtIntersection ? lightAtIntersection[road.to] : -1;
		});

		/* The roads of each flow's route, as road indexes. */
		const flowRoutes = flows.map(function (flow) {
			return routes[indexes.route[flow.route]].roads.map(function (id) { return roadIndex[id]; });
		});

		/* ---- State ---- */

		/** The next tick to simulate, which is also the number of simulated ticks. */
		let tick = 0;
		let nextVehicleId = 1;
		/** The cells of each road: null, or the vehicle that occupies it. */
		let cells = roads.map(function (road) { return new Array(road.cells).fill(null); });
		/** The maximum number of occupied cells of each road, at the end of a tick. */
		const maxOccupied = roads.map(function () { return 0; });
		/** The current phase of each light (an index) and its remaining seconds. */
		const lightStates = lights.map(function (light) {
			return { phase: 0, remaining: light.phases.length > 0 ? light.phases[0].duration : 0 };
		});
		/** The vehicles emitted by each flow, and those still waiting to enter. */
		const flowStates = flows.map(function () { return { emitted: 0, waiting: [] }; });
		/** The accumulator of each metric (§4.6). */
		const accumulators = metrics.map(function () {
			return { count: 0, sum: 0, minimum: null, maximum: null };
		});
		/** The events of the tick in progress (phase changes, arrivals and logs). */
		let events = [];
		/** The light whose policy is running, for extend(); -1 outside policies. */
		let activeLight = -1;

		/* ---- Queries (§3.6 and D18) ---- */

		function occupiedCells(r) {
			let count = 0;
			cells[r].forEach(function (vehicle) {
				if (vehicle !== null) {
					++count;
				}
			});
			return count;
		}

		/** ⌊100 · occupied / cells⌋. */
		function occupancy(r) {
			return Math.floor(100 * occupiedCells(r) / roads[r].cells);
		}

		/**
		 * The contiguous block of stopped vehicles, counted from the end of the
		 * road (D18).
		 */
		function queue(r) {
			const row = cells[r];
			let count = 0;
			for (let p = row.length - 1; 0 <= p && row[p] !== null && row[p].stopped; --p) {
				++count;
			}
			return count;
		}

		/** True if the road may cross its end: no light there, or the current phase enables it. */
		function isGreen(r) {
			const l = lightOfRoad[r];
			if (l < 0) {
				return true;
			}
			const phases = lights[l].phases;
			return phases.length > 0 && phases[lightStates[l].phase].roads.indexOf(roads[r].id) >= 0;
		}

		/** The label of an entity or, if it has none, its id. */
		function nameOf(entityClass, id) {
			if (!(entityClass in indexes) || !(id in indexes[entityClass])) {
				return id;
			}
			const label = entities[entityClass][indexes[entityClass][id]].label;
			return label !== null && label !== undefined ? label : id;
		}

		/* ---- The $ctx object of the generated logic (§5.3) ---- */

		const ctx = {
			elapsed: 0,
			queue: function (id) { return queue(roadIndex[id]); },
			occupancy: function (id) { return occupancy(roadIndex[id]); },
			extend: function (id, seconds) {
				if (activeLight < 0 || lights[activeLight].phases.length === 0) {
					return;
				}
				const state = lightStates[activeLight];
				if (lights[activeLight].phases[state.phase].roads.indexOf(id) >= 0) {
					state.remaining += seconds;
				}
			},
			log: function (text) { events.push({ kind: 'log', tick: tick, text: String(text) }); },
			name: nameOf,
			fmt: function (type, value) { return formatValue(type, value, nameOf); },
			/* Truncated integer division, and 0 if b = 0 (D21). "|| 0" turns a -0 into 0. */
			div: function (a, b) { return b === 0 ? 0 : (Math.trunc(a / b) || 0); }
		};

		/* ---- Metrics (§4.6) ---- */

		/** Evaluates the metrics of an event ("tick" or "arrival"), in order. */
		function evaluateMetrics(event, trip) {
			metrics.forEach(function (metric, m) {
				if (metric.event !== event) {
					return;
				}
				const evaluate = metricFunctions[m];
				const accumulator = accumulators[m];
				++accumulator.count;
				if (typeof evaluate !== 'function') {
					return;     /* count() has no argument: its function is null. */
				}
				const value = event === 'arrival' ? evaluate(ctx, trip) : evaluate(ctx);
				accumulator.sum += value;
				if (accumulator.minimum === null || value < accumulator.minimum) {
					accumulator.minimum = value;
				}
				if (accumulator.maximum === null || value > accumulator.maximum) {
					accumulator.maximum = value;
				}
			});
		}

		function metricValue(m) {
			const accumulator = accumulators[m];
			switch (metrics[m].aggregator) {
				case 'count': return accumulator.count;
				case 'sum': return accumulator.sum;
				case 'average': return accumulator.count === 0 ? null : accumulator.sum / accumulator.count;
				case 'minimum': return accumulator.minimum;
				case 'maximum': return accumulator.maximum;
				default: return null;
			}
		}

		/* ---- The steps of a tick (§4.4) ---- */

		/** 1. Emission: each flow adds "spawn" vehicles to its external queue. */
		function emit(t) {
			flows.forEach(function (flow, f) {
				if (flow.from <= t && t < Math.min(flow.to, duration) && (t - flow.from) % flow.every === 0) {
					for (let k = 0; k < flow.spawn; ++k) {
						flowStates[f].waiting.push({
							id: nextVehicleId++,
							flow: f,
							route: flowRoutes[f],
							leg: 0,         /* the index of its current road in the route */
							acc: 0,         /* the accumulator, in 1/3600 m (§4.2) */
							born: t,        /* the tick of its emission (D17) */
							stopped: false  /* see D18 and move() */
						});
					}
					flowStates[f].emitted += flow.spawn;
				}
			});
		}

		/** 2. Policies: every "apply" of every light, in order. */
		function runPolicies() {
			lights.forEach(function (light, l) {
				(light.apply || []).forEach(function (application) {
					const policy = policies[application.policy];
					if (typeof policy === 'function') {
						activeLight = l;
						policy.apply(null, [ctx].concat(application.args));
					}
				});
			});
			activeLight = -1;
		}

		/**
		 * 3. Movement (§4.2 and §4.3). Every decision is taken on the state at the
		 * beginning of the tick ("before"), and the new state is built apart, so
		 * the order of the vehicles of a road does not matter. The roads are
		 * visited in declaration order, which gives the priority of the crossings.
		 */
		function move(t) {
			const before = cells;
			const after = roads.map(function (road) { return new Array(road.cells).fill(null); });
			/* True if a crossing already claimed cell 0 of the road in this tick. */
			const claimed = roads.map(function () { return false; });
			const arrivals = [];

			roads.forEach(function (road, r) {
				const last = road.cells - 1;
				before[r].forEach(function (vehicle, p) {
					if (vehicle === null) {
						return;
					}
					vehicle.acc += road.limit;
					const wanted = Math.floor(vehicle.acc / CELL);
					let free = 0;
					while (p + free < last && before[r][p + free + 1] === null) {
						++free;
					}
					let advanced = Math.min(wanted, free);
					let destination = { road: r, cell: p + advanced };
					/* At the end of the road with advance left: it tries to cross (one cell more). */
					if (advanced < wanted && p + advanced === last && isGreen(r)) {
						if (vehicle.leg === vehicle.route.length - 1) {
							++advanced;
							destination = null;     /* it leaves the network */
							arrivals.push({ vehicle: vehicle, travelTime: t + 1 - vehicle.born });
						}
						else {
							const next = vehicle.route[vehicle.leg + 1];
							if (before[next][0] === null && !claimed[next]) {
								claimed[next] = true;
								++advanced;
								++vehicle.leg;
								destination = { road: next, cell: 0 };
							}
						}
					}
					if (advanced === wanted) {
						vehicle.acc -= advanced * CELL;
					}
					else {
						vehicle.acc = 0;            /* it was blocked */
					}
					/* D18: stopped if it wanted to advance and did not advance any cell. With
					 * wanted = 0 (the accumulator has not reached a cell yet) it is not stopped. */
					vehicle.stopped = 1 <= wanted && advanced === 0;
					if (destination !== null) {
						after[destination.road][destination.cell] = vehicle;
					}
				});
			});
			cells = after;

			arrivals.forEach(function (arrival) {
				events.push({
					kind: 'arrival',
					tick: t,
					vehicle: arrival.vehicle.id,
					flow: flows[arrival.vehicle.flow].id,
					travelTime: arrival.travelTime
				});
				evaluateMetrics('arrival', { travel_time: arrival.travelTime });
			});
		}

		/** 4. Ingress: each flow inserts at most one waiting vehicle into cell 0 of its first road. */
		function enter() {
			flows.forEach(function (flow, f) {
				const waiting = flowStates[f].waiting;
				const first = flowRoutes[f][0];
				if (waiting.length > 0 && cells[first][0] === null) {
					const vehicle = waiting.shift();
					vehicle.stopped = false;
					cells[first][0] = vehicle;
				}
			});
		}

		/** 5. Lights: the remaining time decreases, and at 0 the next phase starts (cyclically). */
		function advanceLights(t) {
			lights.forEach(function (light, l) {
				const state = lightStates[l];
				if (light.phases.length === 0) {
					return;
				}
				--state.remaining;
				if (state.remaining <= 0) {
					state.phase = (state.phase + 1) % light.phases.length;
					state.remaining = light.phases[state.phase].duration;
					events.push({ kind: 'phase', tick: t, light: light.id, phase: state.phase });
				}
			});
		}

		/** Simulates the tick t = tick (§4.4), and returns its events. */
		function step() {
			if (tick >= duration) {
				return [];
			}
			const t = tick;
			events = [];
			ctx.elapsed = t;
			emit(t);
			runPolicies();
			move(t);
			enter();
			advanceLights(t);
			evaluateMetrics('tick', null);
			roads.forEach(function (road, r) {
				maxOccupied[r] = Math.max(maxOccupied[r], occupiedCells(r));
			});
			tick = t + 1;
			return events;
		}

		/** The state to draw. Each cell is "empty", "moving" or "stopped". */
		function state() {
			return {
				elapsed: tick,
				roads: roads.map(function (road, r) {
					return {
						id: road.id,
						cells: cells[r].map(function (vehicle) {
							return vehicle === null ? 'empty' : (vehicle.stopped ? 'stopped' : 'moving');
						}),
						occupied: occupiedCells(r),
						maxOccupied: maxOccupied[r],
						green: lightOfRoad[r] < 0 ? null : isGreen(r)
					};
				}),
				lights: lights.map(function (light, l) {
					return { id: light.id, phase: lightStates[l].phase, remaining: lightStates[l].remaining };
				}),
				flows: flows.map(function (flow, f) {
					return { id: flow.id, emitted: flowStates[f].emitted, waiting: flowStates[f].waiting.length };
				})
			};
		}

		/** The value of every metric, in declaration order. */
		function metricValues() {
			return metrics.map(function (metric, m) {
				return {
					id: metric.id,
					label: metric.label,
					aggregator: metric.aggregator,
					event: metric.event,
					value: metricValue(m),
					unit: metric.unit
				};
			});
		}

		/**
		 * The content of "Exportar JSON" (§5.4). The values are in base units,
		 * except the speeds, which go in km/h (and may have decimals), as their
		 * unit says.
		 */
		function report() {
			return {
				simulation: {
					id: config.simulation.id,
					label: config.simulation.label,
					duration: duration
				},
				elapsed: tick,
				metrics: metricValues().map(function (metric) {
					if (metric.unit === 'km/h' && metric.value !== null) {
						metric.value = metric.value / 1000;
					}
					return metric;
				})
			};
		}

		return {
			config: config,
			step: step,
			done: function () { return tick >= duration; },
			elapsed: function () { return tick; },
			state: state,
			metrics: metricValues,
			report: report,
			name: nameOf
		};
	}

	return {
		CELL: CELL,
		createSimulation: createSimulation,
		formatValue: formatValue
	};
})();

/* ========================================================================== */
/* 2. VIEWER                                                                  */
/* ========================================================================== */

if (typeof document !== 'undefined') {
	(function () {
		'use strict';

		const configElement = document.getElementById('vial-config');
		if (configElement === null) {
			return;
		}
		const config = JSON.parse(configElement.textContent);
		const logic = typeof VIAL_LOGIC !== 'undefined' ? VIAL_LOGIC : { policies: {}, metrics: [] };
		const simulation = config.simulation;
		const duration = simulation.duration;
		const intersections = config.intersections || [];
		const roads = config.roads || [];
		const lights = config.lights || [];
		const routes = config.routes || [];
		const flows = config.flows || [];
		const metrics = config.metrics || [];
		const formatValue = VialEngine.formatValue;

		/* ---- Helpers ---- */

		/* A map by id, with no prototype (see indexById in the engine). */
		function byId(elements) {
			const map = Object.create(null);
			elements.forEach(function (element) { map[element.id] = element; });
			return map;
		}
		const intersectionById = byId(intersections);
		const roadById = byId(roads);
		const routeById = byId(routes);
		const flowById = byId(flows);
		const lightById = byId(lights);
		const lightAt = Object.create(null);
		lights.forEach(function (light) { lightAt[light.at] = light; });

		/** The visible name of an entity: its label or, if it has none, its id. */
		function display(entity) {
			return entity.label !== null && entity.label !== undefined ? entity.label : entity.id;
		}

		function roadNames(ids) {
			return ids.map(function (id) { return display(roadById[id]); }).join(', ');
		}

		function element(tag, className, parent) {
			const node = document.createElement(tag);
			node.className = className;
			parent.appendChild(node);
			return node;
		}

		const svgNamespace = 'http://www.w3.org/2000/svg';
		function svgElement(tag, attributes, parent) {
			const node = document.createElementNS(svgNamespace, tag);
			Object.keys(attributes).forEach(function (name) { node.setAttribute(name, attributes[name]); });
			if (parent) {
				parent.appendChild(node);
			}
			return node;
		}

		/** A time in seconds as "mm:ss", or as "h:mm:ss" if the simulation lasts 1 h or more. */
		const withHours = 3600 <= duration;
		function twoDigits(n) {
			return (n < 10 ? '0' : '') + n;
		}
		function clock(seconds) {
			const s = twoDigits(seconds % 60);
			if (withHours) {
				return Math.floor(seconds / 3600) + ':' + twoDigits(Math.floor((seconds % 3600) / 60)) + ':' + s;
			}
			return twoDigits(Math.floor(seconds / 60)) + ':' + s;
		}

		/** "1 calle", "2 calles". */
		function amount(count, singular, plural) {
			return count + ' ' + (count === 1 ? singular : plural);
		}

		/** ⌊100 · part / whole⌋, as the occupancy of the language. */
		function percent(part, whole) {
			return Math.floor(100 * part / whole);
		}

		document.title = 'Vial · ' + display(simulation);
		document.getElementById('scen').textContent = ' · ' + display(simulation);

		/* ---- Map ---- */

		/*
		 * The map is drawn in meters. The coordinates are Cartesian (Y up) and the
		 * SVG has Y down, so the screen position of an intersection is (x, −y).
		 */
		const position = Object.create(null);
		intersections.forEach(function (n) { position[n.id] = [n.x, -n.y]; });

		const svg = document.getElementById('map');
		const cellRects = [];       /* the rectangles of the cells of each road */
		const cellClasses = [];     /* their current class, to touch the DOM only on changes */
		const lightDots = [];       /* { road: index, element } */

		function distanceBetween(a, b) {
			return Math.sqrt((b[0] - a[0]) * (b[0] - a[0]) + (b[1] - a[1]) * (b[1] - a[1]));
		}

		/*
		 * Layout:
		 *  - D is the size, in meters, of one "design pixel". Every fixed-size
		 *    element (texts, nodes, arrows, dots, margins and offsets) is its size
		 *    in design pixels times D, and the map is about MAP_WIDTH design pixels
		 *    wide, so they look the same whatever the size of the network.
		 *  - Each road keeps a margin M0 at its origin and M1 at its destination
		 *    (at most a fraction of the road, for very short roads).
		 *  - The cells have a single size for the whole map: the largest one that
		 *    fits in the tightest road, between its margins. The run of cells
		 *    starts at the origin of the road, and what is left is a thin approach
		 *    to the destination, where the light is.
		 */
		const MAP_WIDTH = 800, PAD_X = 70, PAD_TOP = 58, PAD_BOTTOM = 68, M0 = 30, M1 = 46;
		const xs = intersections.map(function (n) { return position[n.id][0]; });
		const ys = intersections.map(function (n) { return position[n.id][1]; });
		const x0 = Math.min.apply(null, xs), x1 = Math.max.apply(null, xs);
		const y0 = Math.min.apply(null, ys), y1 = Math.max.apply(null, ys);
		const extent = Math.max(x1 - x0, y1 - y0);
		const D = extent > 0 ? extent / (MAP_WIDTH - 2 * PAD_X) : 1;

		function marginsOf(length) {
			return [Math.min(M0 * D, 0.2 * length), Math.min(M1 * D, 0.3 * length)];
		}
		let pitch = Infinity;       /* the length of a cell plus its gap */
		roads.forEach(function (road) {
			const length = distanceBetween(position[road.from], position[road.to]);
			const margins = marginsOf(length);
			pitch = Math.min(pitch, (length - margins[0] - margins[1]) / road.cells);
		});
		const gap = pitch / 12;
		const cellWidth = pitch - gap;

		(function buildMap() {
			if (intersections.length === 0) {
				svg.style.display = 'none';
				document.getElementById('legend').style.display = 'none';
				element('div', 'empty mapmsg', svg.parentNode).textContent =
					'Sin red vial: el programa no declara intersecciones.';
				return;
			}
			roads.forEach(function (road, r) {
				const a = position[road.from], b = position[road.to];
				const dx = b[0] - a[0], dy = b[1] - a[1], len = distanceBetween(a, b);
				const ux = dx / len, uy = dy / len, nx = -uy, ny = ux;
				const s0 = marginsOf(len)[0];
				const run = road.cells * pitch - gap;
				/* In the group, the road goes from (0, 0) to (len, 0). */
				const group = svgElement('g', {
					transform: 'translate(' + a[0] + ' ' + a[1] + ') rotate(' + (Math.atan2(dy, dx) * 180 / Math.PI) + ')'
				}, svg);
				/* The lane is offset only if the opposite road exists (two-way). */
				const twoWay = roads.some(function (o) { return o !== road && o.from === road.to && o.to === road.from; });
				const off = twoWay ? 10 * D : 0;
				svgElement('line', { x1: 0, y1: off, x2: s0 - 2 * D, y2: off, 'class': 'conn' }, group);
				svgElement('line', { x1: s0 + run + 2 * D, y1: off, x2: len - 22 * D, y2: off, 'class': 'conn' }, group);
				svgElement('polygon', {
					points: (len - 12 * D) + ',' + off + ' ' + (len - 23 * D) + ',' + (off - 6.5 * D) + ' ' +
						(len - 23 * D) + ',' + (off + 6.5 * D),
					'class': 'arrow'
				}, group);
				cellRects[r] = [];
				cellClasses[r] = [];
				for (let i = 0; i < road.cells; ++i) {
					cellRects[r].push(svgElement('rect', {
						x: s0 + i * pitch, y: off - 5 * D, width: cellWidth, height: 10 * D,
						rx: Math.min(2 * D, cellWidth / 3), 'class': 'c-emp'
					}, group));
					cellClasses[r].push('c-emp');
				}
				const middle = s0 + run / 2;
				const vertical = Math.abs(uy) > 0.7;      /* vertical road: the name goes aside */
				const lo = vertical ? off + 12 * D : off + 22 * D;
				svgElement('text', {
					x: a[0] + ux * middle + nx * lo,
					y: a[1] + uy * middle + ny * lo + (vertical ? 4 * D : 0),
					'text-anchor': vertical ? (nx >= 0 ? 'start' : 'end') : 'middle',
					'class': 'slab',
					style: 'font-size:' + (11.5 * D) + 'px'
				}, svg).textContent = display(road);
				if (road.to in lightAt) {
					const distance = s0 + run + 12 * D;
					lightDots.push({
						road: r,
						element: svgElement('circle', {
							cx: a[0] + ux * distance + nx * off, cy: a[1] + uy * distance + ny * off,
							r: 5.5 * D, 'class': 'd-rojo'
						}, svg)
					});
				}
			});
			intersections.forEach(function (n) {
				const p = position[n.id];
				svgElement('rect', {
					x: p[0] - 9 * D, y: p[1] - 9 * D, width: 18 * D, height: 18 * D, rx: 4 * D, 'class': 'nodo'
				}, svg);
				/* The name goes below the node if some road arrives from above. */
				const above = roads.some(function (road) {
					return (road.from === n.id && position[road.to][1] < p[1]) ||
						(road.to === n.id && position[road.from][1] < p[1]);
				});
				svgElement('text', {
					x: p[0], y: p[1] + (above ? 34 : -19) * D, 'text-anchor': 'middle', 'class': 'nlab',
					style: 'font-size:' + (12 * D) + 'px'
				}, svg).textContent = display(n);
			});
			/* At least MAP_WIDTH design pixels wide, centered, so a narrow network is not enlarged. */
			const width = Math.max(x1 - x0 + 2 * PAD_X * D, MAP_WIDTH * D);
			svg.setAttribute('viewBox', ((x0 + x1 - width) / 2) + ' ' + (y0 - PAD_TOP * D) + ' ' +
				width + ' ' + (y1 - y0 + (PAD_TOP + PAD_BOTTOM) * D));
		})();

		/* ---- Panels ---- */

		const lightRows = Object.create(null);     /* light id → its phase rows */
		const metricValues = [];    /* { number, unit } elements of each metric value */
		const roadBars = [];        /* { fill, mark, value } of each road */
		const flowLive = [];        /* the live line of each flow */

		(function buildPanels() {
			const lightPanel = document.getElementById('lpanel');
			if (lights.length === 0) {
				element('div', 'empty', lightPanel).textContent = 'Sin semáforos';
			}
			lights.forEach(function (light) {
				const box = element('div', 'light', lightPanel);
				element('div', 'light-h', box).textContent =
					display(light) + ' · en ' + display(intersectionById[light.at]);
				if (light.phases.length === 0) {
					element('div', 'empty', box).textContent = 'Sin fases';
				}
				lightRows[light.id] = light.phases.map(function (phase, i) {
					const row = element('div', 'ph', box);
					element('span', 'phdot', row);
					element('span', 'phtxt', row).textContent = 'fase ' + (i + 1) + ' · verde ' + roadNames(phase.roads);
					return { row: row, remaining: element('span', 'phrem mono', row), duration: phase.duration };
				});
			});

			const metricPanel = document.getElementById('m-program');
			if (metrics.length === 0) {
				element('div', 'empty', metricPanel).textContent = 'Sin métricas';
			}
			metrics.forEach(function (metric) {
				const box = element('div', '', metricPanel);
				const value = element('div', 'big mono', box);
				metricValues.push({ number: element('span', '', value), unit: element('small', '', value) });
				element('div', 'lbl', box).textContent = display(metric);
			});

			const roadPanel = document.getElementById('m-streets');
			if (roads.length === 0) {
				element('div', 'empty', roadPanel).textContent = 'Sin calles';
			}
			roads.forEach(function (road) {
				const row = element('div', 'strow', roadPanel);
				const name = element('span', '', row);
				name.textContent = display(road);
				name.title = display(road);
				const bar = element('div', 'bar', row);
				roadBars.push({ fill: element('i', '', bar), mark: element('em', '', bar), value: element('span', 'val mono', row) });
			});

			const flowPanel = document.getElementById('fpanel');
			if (flows.length === 0) {
				element('div', 'empty', flowPanel).textContent = 'Sin flujos';
			}
			flows.forEach(function (flow) {
				const row = element('div', 'flrow', flowPanel);
				/* The route as its intersections: the origin of its first road, and then each destination. */
				const route = routeById[flow.route].roads.map(function (id) { return roadById[id]; });
				const stops = [route[0].from].concat(route.map(function (road) { return road.to; }));
				element('div', 'flname', row).textContent = display(flow) + ' · ' +
					stops.map(function (id) { return display(intersectionById[id]); }).join(' → ');
				flowLive.push(element('span', 'fllive mono', row));
			});
		})();

		/**
		 * A metric value with its unit: "—" if it is null, and an average with one
		 * decimal. The speeds come in m/h and are shown in km/h.
		 */
		function metricText(metric) {
			if (metric.value === null) {
				return { value: '—', unit: '' };
			}
			const unit = metric.unit !== null ? ' ' + metric.unit : '';
			if (metric.aggregator === 'average') {
				const value = metric.unit === 'km/h' ? metric.value / 1000 : metric.value;
				return { value: value.toFixed(1), unit: unit };
			}
			if (metric.unit === 'km/h') {
				/* The format of §3.8 ("40 km/h", "4.3 km/h"), without its unit. */
				const text = formatValue('speed', metric.value);
				return { value: text.slice(0, text.length - ' km/h'.length), unit: unit };
			}
			return { value: String(metric.value), unit: unit };
		}

		/* ---- Trace ---- */

		const trace = document.getElementById('trace');
		const TRACE_LINES = 150;

		function traceLine(time, kind, text) {
			const line = document.createElement('div');
			line.className = 'ln';
			element('span', 'tk', line).textContent = clock(time) + ' ';
			element('span', 't-' + kind, line).textContent = '[' + kind + '] ';
			element('span', '', line).textContent = text;
			trace.insertBefore(line, trace.firstChild);
			while (trace.childNodes.length > TRACE_LINES) {
				trace.removeChild(trace.lastChild);
			}
		}

		function traceEvent(event) {
			if (event.kind === 'phase') {
				const light = lightById[event.light];
				traceLine(event.tick, 'fase', display(light) + ' → fase ' + (event.phase + 1) +
					' (verde ' + roadNames(light.phases[event.phase].roads) + ')');
			}
			else if (event.kind === 'arrival') {
				traceLine(event.tick, 'veh', 'v' + event.vehicle + ' de ' + display(flowById[event.flow]) +
					' completa su ruta en ' + formatValue('duration', event.travelTime));
			}
			else if (event.kind === 'log') {
				traceLine(event.tick, 'log', event.text);
			}
		}

		/* ---- Simulation and drawing ---- */

		let sim = null;
		let running = false;

		function render() {
			const state = sim.state();
			state.roads.forEach(function (road, r) {
				if (cellRects[r] !== undefined) {
					road.cells.forEach(function (cell, i) {
						const className = cell === 'empty' ? 'c-emp' : (cell === 'moving' ? 'c-mov' : 'c-det');
						if (cellClasses[r][i] !== className) {
							cellClasses[r][i] = className;
							cellRects[r][i].setAttribute('class', className);
						}
					});
				}
				const cells = roads[r].cells, bar = roadBars[r];
				bar.fill.style.width = (100 * road.occupied / cells) + '%';
				bar.mark.style.left = 'calc(' + (100 * road.maxOccupied / cells) + '% - 1px)';
				bar.value.textContent = percent(road.occupied, cells) + '% · máx ' + percent(road.maxOccupied, cells) + '%';
			});
			lightDots.forEach(function (dot) {
				dot.element.setAttribute('class', state.roads[dot.road].green ? 'd-verde' : 'd-rojo');
			});
			state.lights.forEach(function (light) {
				lightRows[light.id].forEach(function (row, i) {
					const on = i === light.phase;
					row.row.className = 'ph' + (on ? ' on' : '');
					row.remaining.textContent = (on ? light.remaining : row.duration) + ' s';
				});
			});
			sim.metrics().forEach(function (metric, m) {
				const text = metricText(metric);
				metricValues[m].number.textContent = text.value;
				metricValues[m].unit.textContent = text.unit;
			});
			state.flows.forEach(function (flow, f) {
				flowLive[f].textContent = 'emitidos ' + flow.emitted + ' · en espera ' + flow.waiting;
			});
			document.getElementById('tickbox').textContent = clock(state.elapsed) + ' / ' + clock(duration);
		}

		/** Simulates one tick, and stops at the end. */
		function step() {
			if (sim.done()) {
				return;
			}
			sim.step().forEach(traceEvent);
			if (sim.done()) {
				traceLine(sim.elapsed(), 'sis', 'fin de la simulación');
				running = false;
				syncButtons();
			}
		}

		function reset() {
			sim = VialEngine.createSimulation(config, logic);
			trace.textContent = '';
			traceLine(0, 'sis', 'simulación ' + display(simulation) + ' cargada · ' + [
				amount(intersections.length, 'intersección', 'intersecciones'),
				amount(roads.length, 'calle', 'calles'),
				amount(lights.length, 'semáforo', 'semáforos'),
				amount(flows.length, 'flujo', 'flujos')
			].join(' · '));
			render();
		}

		/* ---- Loop: the simulated time per real second is the multiplier (×1 … ×60) ---- */

		const speed = document.getElementById('speed');
		let last = 0, pending = 0;
		function frame(timestamp) {
			requestAnimationFrame(frame);
			if (!running) {
				last = timestamp;
				return;
			}
			if (!last) {
				last = timestamp;
			}
			const seconds = Math.min((timestamp - last) / 1000, 0.5);
			last = timestamp;
			pending += seconds * parseFloat(speed.value);
			const ticks = Math.floor(pending);
			pending -= ticks;
			if (ticks > 0) {
				for (let i = 0; i < ticks && running; ++i) {
					step();
				}
				render();
			}
		}

		/* ---- Controls ---- */

		const runButton = document.getElementById('b-run');
		const stepButton = document.getElementById('b-step');
		function syncButtons() {
			runButton.textContent = running ? 'Pausar' : 'Reanudar';
			runButton.disabled = sim.done();
			stepButton.disabled = sim.done();
		}
		runButton.addEventListener('click', function () {
			if (!sim.done()) {
				running = !running;
				syncButtons();
			}
		});
		stepButton.addEventListener('click', function () {
			running = false;
			step();
			render();
			syncButtons();
		});
		document.getElementById('b-reset').addEventListener('click', function () {
			reset();
			syncButtons();
		});
		document.getElementById('b-export').addEventListener('click', function () {
			const blob = new Blob([JSON.stringify(sim.report(), null, 2)], { type: 'application/json' });
			const link = document.createElement('a');
			link.href = URL.createObjectURL(blob);
			link.download = simulation.id + '-metricas.json';
			document.body.appendChild(link);
			link.click();
			link.remove();
			setTimeout(function () { URL.revokeObjectURL(link.href); }, 1000);
		});

		reset();
		running = true;
		syncButtons();
		requestAnimationFrame(frame);
	})();
}

if (typeof module !== 'undefined') {
	module.exports = VialEngine;
}
