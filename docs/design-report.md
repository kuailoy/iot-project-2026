## 1. Opportunity Statement

There is an opportunity for a small, low-distraction desktop device that shows environmental information and useful daily information such as weather, calendar, tasks, and habits. Unlike a phone or tablet, the device should be glanceable, quiet, and always visible without demanding constant interaction.

The device is intended to combine:

- temperature and humidity monitoring,
- optional air-quality monitoring,
- E-Ink display,
- Wi-Fi connectivity,
- weather, calendar, and task/habit reminders,
- and possibly Spotify album artwork as a showcase feature.

The core hypothesis is that users want useful information at a glance without another noisy screen.

---

## 2. Design Model Used This Week

This draft mainly uses the **simple four-stage model** from Cross:

1. **Exploration** – What are we trying to design?
2. **Generation** – How could we do it?
3. **Evaluation** – How well does each option work?
4. **Communication** – How do we document and present the design?

### Model mapping

| Four-stage model | Current project work |
|---|---|
| Exploration | Identifying user needs, desktop context, and information overload |
| Generation | Creating concepts: basic dashboard, color photo frame, black-and-white Spotify display |
| Evaluation | Comparing refresh rate, power, cost, complexity, and emotional value |
| Communication | This document, GitHub issues, diagrams, and component lists |

In later iterations, **VDI 2221**, **French’s model**, **Archer’s model**, and **March’s model** will also be used to show at least three different design models, as required by the grading criteria.

---

## 3. Exploration: Problem Space

### Context

Students and remote workers receive important information through many channels: learning platforms, email, Teams, student portals, calendar apps, task apps, and classroom announcements. Important and trivial messages often look the same. Users may miss deadlines, room changes, or cancelled events even when the information was announced somewhere.

### Target users

- Students who need to follow schedules, deadlines, and daily tasks.
- Remote workers who want a calm desktop information display.
- People interested in environmental conditions such as temperature, humidity, and air quality.
- Users who appreciate minimal, attractive, low-power devices.

### User pain points

- Phone notifications are distracting.
- Information is fragmented across multiple apps.
- Checking the phone for one item often leads to unintended scrolling.
- Important information is not always visible at a glance.
- Existing smart displays can be expensive, power-hungry, or privacy-invasive.

### Constraints

- The device must be small and attractive on a desk.
- It should consume low power.
- It should update without constant user interaction.
- It should work over Wi-Fi.
- It should respect privacy and avoid unnecessary data collection.
- E-Ink refresh rate limits dynamic content.
- Cost and component availability matter.

---

## 4. Generation: Solution Space

### Concept A – Basic E-Ink Information Dashboard

A black-and-white E-Ink display shows weather, temperature, humidity, calendar, tasks, and habits.

**Strengths:** simple, low power, calm, feasible.  
**Weaknesses:** may feel purely functional and less emotionally engaging.

### Concept B – Color E-Ink Photo Frame with Information

A color E-Ink display works mainly as a personal photo frame while also showing weather and environmental data.

**Strengths:** attractive, emotional, good for desk decoration.  
**Weaknesses:** color E-Ink refreshes slowly, around 20–35 seconds, which is not ideal for dynamic information.

### Concept C – Black-and-White E-Ink with Spotify Album Art

The device shows the currently playing song, artist, album artwork, and playback status. Album artwork is converted to black and white. Real-time playback progress is not required.

**Strengths:** demonstrates cloud API + IoT + physical display integration.  
**Weaknesses:** limited to Spotify users; may be a showcase feature rather than a core need.

### Concept D – Environmental Sensing Device

The device focuses on temperature, humidity, and optional air quality, with a simple E-Ink readout.

**Strengths:** clear IoT sensing purpose.  
**Weaknesses:** less useful as a daily information companion unless combined with calendar/tasks.

---

## 5. Co-evolution of Problem and Solution Spaces

The project shows how the problem and solution spaces evolve together.

| Stage | Problem space | Solution space |
|---|---|---|
| PS1 / SS1 | Daily information is fragmented | Basic E-Ink dashboard |
| PS2 / SS2 | Phone notifications are distracting | Low-distraction desktop display |
| PS3 / SS3 | Users want emotional value, not only utility | Color E-Ink photo frame |
| PS4 / SS4 | Color E-Ink is too slow for dynamic content | Black-and-white E-Ink with Spotify album art |
| PS5 / SS5 | Users also care about their environment | Add temperature, humidity, optional air quality |

This co-evolution means the design is not a straight line. The problem understanding changes as the solution possibilities become clearer.

---

## 6. User Scenarios and Personas

### Persona 1: Aino, 22 – First-year IT student

Aino checks her phone constantly because course information is spread across the learning platform, email, Teams, and the student portal. She often misses small but important updates. She wants a calm desk device that shows the next deadline, today’s schedule, and weather without requiring her to open her phone.

### Persona 2: Matti, 34 – Remote worker

Matti works from home and wants a minimal desk companion. He cares about indoor temperature, humidity, and air quality. He does not want another screen that demands attention. He would use the device to glance at weather, calendar, and tasks during work.

### Scenario

It is Monday morning. Aino sits at her desk. The device shows the weather, her first lecture room, two tasks for the day, and the indoor humidity. She notices that the air quality is slightly poor and opens a window. She does not need to unlock her phone. Later, while listening to Spotify, the device shows the album artwork in black and white.

This scenario will be tested through user interviews and observation in the next iteration.

---

## 7. Requirements

### Functional requirements

- Display weather information.
- Display temperature and humidity.
- Optional: display air-quality information.
- Display calendar events and tasks.
- Display habit reminders.
- Connect over Wi-Fi.
- Optional: show Spotify track and album artwork.
- Support low-power E-Ink updates.

### Non-functional requirements

- Attractive desktop appearance.
- Low distraction.
- Low power consumption.
- Stable and reliable operation.
- Quiet, no unnecessary notifications.
- Privacy-respecting data handling.
- Affordable component cost.
- Repairable and easy to prototype.

---

## 8. Evaluation and Decision

A preliminary evaluation matrix was used.

| Criterion | Concept A: Basic dashboard | Concept B: Color photo frame | Concept C: B&W Spotify | Concept D: Environmental sensor |
|---|---|---|---|---|
| Low distraction | High | Medium | Medium | High |
| Emotional value | Medium | High | High | Low |
| Refresh-rate suitability | High | Low | High | High |
| Power consumption | Low | Medium | Low | Low |
| Technical feasibility | High | Medium | Medium | High |
| Cost | Low | Medium | Medium | Low |
| Overall | Strong base | Deferred | Showcase feature | Core sensing add-on |

### Decision

The main direction is a **black-and-white E-Ink information device** with environmental sensing and optional Spotify album-art display. Color E-Ink is deferred because its slow refresh rate is not ideal for dynamic information. The Spotify feature is kept as a showcase feature to demonstrate cloud API and IoT integration.

---

## 9. VDI 2221 Decomposition

VDI 2221 breaks the overall problem into sub-problems and sub-solutions.

| VDI level | Project mapping |
|---|---|
| Overall problem | Design a small desktop device for environmental and daily information |
| Sub-problems | Display, computing, connectivity, sensing, power, interaction, software, enclosure |
| Individual problems | Choose E-Ink display, choose Raspberry Pi, choose sensors, design UI, manage battery |
| Individual solutions | Adafruit 5.83" mono E-Ink, Pi Zero 2 W, rotary encoder, Li-Po battery, 3D-printed case |
| Sub-solutions | Display subsystem, computing subsystem, sensing subsystem, power subsystem, interaction subsystem |
| Overall solution | Integrated IoT desktop information device |

---

## 10. System Architecture and Components

### Hardware

- **Display first choice:** Adafruit 5.83" 648×480 Monochrome E-Ink – Product 6397
- **Display alternative:** Adafruit 7.5" 800×480 Monochrome E-Ink – Product 6396
- **E-Ink driver/adapter:** Adafruit E-Ink Bonnet for Raspberry Pi – Product 6418
- **Computer:** Raspberry Pi Zero 2 W
- **Interaction:** rotary encoder + push button, optional additional buttons
- **Sensors:** temperature/humidity sensor, optional air-quality sensor
- **Other:** microSD card, 3.7V rechargeable Li-Po battery, battery charging/power management module, 5V boost converter, GPIO header, 3D-printed enclosure

### Software

- Python / Linux on Raspberry Pi
- Wi-Fi connectivity
- Weather API
- Calendar and task integration
- Spotify API for current song and album artwork
- Image conversion to black and white
- E-Ink display driver
- Optional sensor reading and logging

---

## 11. RWW Check

### Real

Is the opportunity real? Yes. Many users experience information fragmentation and notification fatigue. A low-distraction desktop display addresses a real need.

### Win

Can we win with this opportunity? Partly. The device must differentiate itself through calm design, E-Ink aesthetics, environmental sensing, and useful integrations.

### Worth it

Is it worth doing? For a course IoT project, yes. It demonstrates sensing, cloud APIs, display output, power management, and physical enclosure design. For a commercial product, more market validation is needed.

---

## 12. Design Practices Applied

### Applied

- **Brainstorming:** generated multiple concepts quickly without early criticism.
- **Synectics:** used analogies such as “digital photo frame” and “calm desk companion.”
- **Problem/solution space enlargement:** reversed the idea from “information dashboard” to “photo frame” and back.
- **User scenarios:** created personas and a Monday-morning scenario.

### Not yet fully applied

- Real user interviews.
- Observation of users in action.
- Quantitative evaluation.
- Power measurement.
- Enclosure prototyping.

These will be addressed in the next iteration.

---

## 13. Risks and Open Questions

- How fast does the selected monochrome E-Ink display refresh in practice?
- How long will the battery last with Wi-Fi and periodic API calls?
- Which sensor is accurate enough but still affordable?
- How should calendar and task data be synced securely?
- How often should the display update to avoid ghosting and power drain?
- Should Spotify be a core feature or only a showcase?
- How can the enclosure look attractive while allowing access to ports and sensors?
- What are the privacy implications of calendar and Spotify integration?

---

## 14. Reflection

This first draft shows that the project is not only a hardware exercise. The key design question is how to provide useful information without creating another distracting screen. Using the four-stage model helped separate exploration from generation. The co-evolution table showed that the problem changed when color E-Ink was found to be too slow. The VDI 2221 decomposition made the system easier to understand because it separated display, computing, sensing, power, and interaction.

What I did not apply yet is enough real user research. The next iteration should include interviews and a simple prototype test. The design process itself also needs to be documented more consistently, because the grading criteria require examples, diagrams, different design models, and reflection.

---

## 15. Next Iteration Plan

1. Interview 2–3 potential users.
2. Test E-Ink refresh rate and readability.
3. Create a simple power budget.
4. Decide whether air quality is core or optional.
5. Sketch the enclosure and desk placement.
6. Build a rough system diagram.
7. Update the document using French’s model or Archer’s model.
8. Add diagrams, photos, and GitHub issue links.
9. Write a short reflection on what changed.

---

This draft is intentionally incomplete. It establishes a baseline for weekly iteration and connects the IoT project to the design models and practices introduced in the lectures.