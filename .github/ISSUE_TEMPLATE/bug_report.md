---
name: Bug report
about: Create a report to help us improve
title: ''
labels: bug
assignees: ''

---

### Bug description
Describe the issue, or paste the full error encountered here.

### How to reproduce
What are the steps to reproduce the reported issue.
```
git clone https://github.com/snuf/iomemory-vsl4.git
cd iomemory-vsl4
git checkout <tag or some-branch>
make module
** poof, broken token **
```

### Possible solution
Is a solution know, or type any plausible suggestions here, if none leave clear.

### Environment information
Information about the system the module is used on
1. Linux kernel compiled against (uname -a)
2. The C compiler version used (gcc --version)
3. distribution, and version (cat /etc/os-release)
4. The hash, tag and/or Branch of iomemory-vsl4 that is being compiled (`git branch -v`)
5. FIO device used, if applicable
   * fio-status
   * lspci -b -nn
