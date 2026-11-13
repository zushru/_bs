function issue(code, message) {
  return { code, message };
}

/**
 * Build a semantic learner-row sequence from a validated month-overlap roster.
 * This contains no Excel coordinates, row heights, subtotal placement or page
 * settings. Those belong to a separately audited template adapter.
 */
export function buildLearnerRows(roster) {
  if (!roster || roster.state !== 'READY'
    || !Array.isArray(roster.male) || !Array.isArray(roster.female)
    || !Number.isSafeInteger(roster.total) || roster.total < 0
    || roster.total !== roster.male.length + roster.female.length) {
    return {
      state: 'BLOCKED',
      rows: [],
      counts: null,
      issues: [issue('INVALID_MONTH_ROSTER', 'A READY month-overlap roster with consistent M/F groups and total is required.')],
    };
  }

  const seen = new Set();
  const rows = [];
  for (const [sex, learners] of [['M', roster.male], ['F', roster.female]]) {
    for (let index = 0; index < learners.length; index += 1) {
      const learner = learners[index];
      if (!learner || !Number.isSafeInteger(learner.studentId) || learner.studentId <= 0
        || learner.sex !== sex || typeof learner.fullName !== 'string' || learner.fullName.trim() === '') {
        return {
          state: 'BLOCKED',
          rows: [],
          counts: null,
          issues: [issue('INVALID_LEARNER_ROW', `The ${sex} roster contains a malformed learner at position ${index + 1}.`)],
        };
      }
      if (seen.has(learner.studentId)) {
        return {
          state: 'BLOCKED',
          rows: [],
          counts: null,
          issues: [issue('DUPLICATE_STUDENT_ID', `Student ID ${learner.studentId} appears more than once in the row plan.`)],
        };
      }
      seen.add(learner.studentId);
      rows.push({
        kind: 'LEARNER',
        key: learner.studentId,
        studentId: learner.studentId,
        sex,
        sexGroupPosition: index + 1,
        fullName: learner.fullName.trim(),
      });
    }
  }

  return {
    state: 'READY',
    rows,
    counts: {
      male: roster.male.length,
      female: roster.female.length,
      total: rows.length,
    },
    issues: [],
  };
}

/**
 * Paginate learner entries using a capacity supplied by a future verified
 * template profile. It deliberately has no default or hard-coded capacity.
 * Summary rows, repeated headings, NLS and signature blocks are not included.
 */
export function paginateLearnerRows(rowPlan, { learnerRowsPerPage } = {}) {
  if (!rowPlan || rowPlan.state !== 'READY' || !Array.isArray(rowPlan.rows)
    || !rowPlan.counts || !Number.isSafeInteger(rowPlan.counts.male)
    || !Number.isSafeInteger(rowPlan.counts.female)
    || !Number.isSafeInteger(rowPlan.counts.total)
    || rowPlan.counts.male < 0 || rowPlan.counts.female < 0
    || rowPlan.counts.total !== rowPlan.rows.length
    || rowPlan.counts.male + rowPlan.counts.female !== rowPlan.counts.total) {
    return {
      state: 'BLOCKED',
      pages: [],
      totalPages: 0,
      issues: [issue('INVALID_LEARNER_ROW_PLAN', 'A READY semantic learner-row plan with consistent counts is required.')],
    };
  }

  const seenIds = new Set();
  let actualMale = 0;
  let actualFemale = 0;
  let femaleGroupStarted = false;
  for (const row of rowPlan.rows) {
    if (!row || row.kind !== 'LEARNER' || !Number.isSafeInteger(row.studentId)
      || row.studentId <= 0 || row.key !== row.studentId
      || typeof row.fullName !== 'string' || row.fullName.trim() === ''
      || (row.sex !== 'M' && row.sex !== 'F')) {
      return {
        state: 'BLOCKED',
        pages: [],
        totalPages: 0,
        issues: [issue('INVALID_LEARNER_ROW_PLAN', 'The learner-row plan contains a malformed row.')],
      };
    }
    if (seenIds.has(row.studentId)) {
      return {
        state: 'BLOCKED',
        pages: [],
        totalPages: 0,
        issues: [issue('DUPLICATE_STUDENT_ID', `Student ID ${row.studentId} appears more than once in the page plan.`)],
      };
    }
    seenIds.add(row.studentId);
    if (row.sex === 'F') {
      femaleGroupStarted = true;
      actualFemale += 1;
    } else {
      if (femaleGroupStarted) {
        return {
          state: 'BLOCKED',
          pages: [],
          totalPages: 0,
          issues: [issue('INVALID_SEX_GROUP_ORDER', 'Male rows must precede female rows in the semantic plan.')],
        };
      }
      actualMale += 1;
    }
  }
  if (actualMale !== rowPlan.counts.male || actualFemale !== rowPlan.counts.female) {
    return {
      state: 'BLOCKED',
      pages: [],
      totalPages: 0,
      issues: [issue('INVALID_LEARNER_ROW_COUNTS', 'Sex-group counts do not match the learner rows.')],
    };
  }

  if (!Number.isSafeInteger(learnerRowsPerPage) || learnerRowsPerPage <= 0) {
    return {
      state: 'BLOCKED',
      pages: [],
      totalPages: 0,
      issues: [issue('INVALID_PAGE_CAPACITY', 'learnerRowsPerPage must be a positive integer measured from a verified template layout.')],
    };
  }

  const pages = [];
  const pageCount = Math.ceil(rowPlan.rows.length / learnerRowsPerPage);
  for (let offset = 0; offset < rowPlan.rows.length; offset += learnerRowsPerPage) {
    const pageIndex = pages.length;
    const rows = rowPlan.rows.slice(offset, offset + learnerRowsPerPage);
    const previousRow = offset > 0 ? rowPlan.rows[offset - 1] : null;
    const nextRow = offset + rows.length < rowPlan.rows.length
      ? rowPlan.rows[offset + rows.length]
      : null;
    pages.push({
      pageNumber: pageIndex + 1,
      totalPages: pageCount,
      rows,
      firstRowOrdinal: offset + 1,
      lastRowOrdinal: offset + rows.length,
      continuesGroupFromPrevious: Boolean(previousRow && previousRow.sex === rows[0].sex),
      continuesGroupOnNext: Boolean(nextRow && nextRow.sex === rows[rows.length - 1].sex),
    });
  }

  return {
    state: 'READY',
    pages,
    totalPages: pages.length,
    totalLearnerRows: rowPlan.rows.length,
    learnerRowsPerPage,
    counts: { ...rowPlan.counts },
    issues: [],
  };
}
