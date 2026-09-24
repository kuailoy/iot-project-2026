# IoT Group Project

This repository is used for managing and developing our group IoT project.

## Workflow

Keep the project management simple:

**If it is a task, create an Issue.**

**If it is code, create a Pull Request.**

**If it is a meeting, add a meeting note.**

Basic workflow:

```text
Issue → Work → Pull Request → Review → Merge
```

Use **Issues** for tasks, problems, ideas, and things that need discussion.

Use **Pull Requests (PRs)** for code changes and other changes that need review.

When a task is completed, close the related Issue.

## Repository Structure

```text
.
├── README.md
├── meetings/          # Meeting notes and decisions
├── docs/              # Project documentation
├── src/               # Source code
└── assets/            # Diagrams, images and other project materials
```

## Project Timeline

| Milestone               | Deadline |
| ----------------------- | -------- |
| Project proposal        | Sep 30      |
| Project plan review.    |.OCT 8        |
| Preliminary technical report | OCT 31      |
| Final technical report    | DEC 8      |
| Final presentation (Trade Fair)     | DEC 10      |

## Basic Git Commands

### Clone the repository

```bash
git clone <repository-url>
cd <repository-name>
```

### Check the current status

```bash
git status
```

### Get the latest changes

```bash
git pull
```

### Create a new branch

```bash
git checkout -b feature/my-feature
```

### Stage changes

```bash
git add .
```

Or stage a specific file:

```bash
git add path/to/file
```

### Commit changes

```bash
git commit -m "Add sensor configuration"
```

### Push your branch

```bash
git push -u origin feature/my-feature
```

### Switch branches

```bash
git checkout main
```

Or:

```bash
git checkout <branch-name>
```

### See branches

```bash
git branch
```

### Useful workflow

```text
git pull
    ↓
Create / switch to branch
    ↓
Make changes
    ↓
git add .
    ↓
git commit
    ↓
git push
    ↓
Create Pull Request
```
