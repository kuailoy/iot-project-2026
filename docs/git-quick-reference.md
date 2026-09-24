# Git Quick Reference

## 1. First-time Setup

### Configure your name

```bash
git config --global user.name "Your Name"
```

### Configure your email

Use the email associated with your GitHub account.

```bash
git config --global user.email "your-email@example.com"
```

### Check your configuration

```bash
git config --global --list
```

---

## 2. Get the Repository

### Clone the repository

```bash
git clone <repository-url>
cd <repository-name>
```

### Check the remote repository

```bash
git remote -v
```

---

## 3. Daily Workflow

### Get the latest changes

Before starting work:

```bash
git pull
```

### Check the current status

```bash
git status
```

### Create a new branch

```bash
git checkout -b feature/my-feature
```

Example:

```bash
git checkout -b feature/temperature-sensor
```

### Switch branches

```bash
git checkout main
```

Or:

```bash
git checkout <branch-name>
```

### See all branches

```bash
git branch
```

---

## 4. Commit Changes

### Check what has changed

```bash
git status
```

### Stage changes

```bash
git add .
```

Or stage a specific file:

```bash
git add path/to/file
```

### Commit

```bash
git commit -m "Add temperature sensor"
```

Keep commit messages short and descriptive.

---

## 5. Submit Code for Review

### Push your branch

First time:

```bash
git push -u origin feature/my-feature
```

After that:

```bash
git push
```

### Create a Pull Request

After pushing your branch:

1. Open the GitHub repository.
2. Open the **Pull Request** page.
3. Create a new PR from your branch to `main`.
4. Link the related Issue.
5. Ask another team member to review it.
6. Merge the PR after review.

Basic workflow:

```text
Issue
  ↓
Create branch
  ↓
Make changes
  ↓
Commit
  ↓
Push
  ↓
Pull Request
  ↓
Review
  ↓
Merge
```

---

## 6. Update Your Branch

Before continuing work, get the latest changes:

```bash
git checkout main
git pull
```

Then switch back to your branch:

```bash
git checkout <branch-name>
```

If necessary, update your branch with the latest `main`:

```bash
git merge main
```

---

## 7. Useful Commands

### See commit history

```bash
git log --oneline
```

### See changed files

```bash
git status
```

### Discard changes in a file

```bash
git restore <file>
```

### Remove a file from staging

```bash
git restore --staged <file>
```

### Delete a local branch

```bash
git branch -d <branch-name>
```

---

## Recommended Workflow for This Project

For most tasks, use:

```bash
git pull
git checkout -b feature/my-feature

# Make changes

git add .
git commit -m "Describe the change"
git push -u origin feature/my-feature
```

Then create a **Pull Request** on GitHub.

> **Don't work directly on `main` unless the change is very small and the team agrees.**
