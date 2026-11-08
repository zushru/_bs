const MONTH = '2026-11';
const PROFILE_ID = 'section-profile-2026-2027';

function daysInMonth(month) {
  const [year, monthNumber] = month.split('-').map(Number);
  return new Date(Date.UTC(year, monthNumber, 0)).getUTCDate();
}

function isoDay(day) {
  return `${MONTH}-${String(day).padStart(2, '0')}`;
}

const classSessions = new Map([
  ['2026-11-02', ['AM', 'PM']],
  ['2026-11-03', ['AM', 'PM']],
  ['2026-11-05', ['AM', 'PM']],
  ['2026-11-06', ['AM', 'PM']],
]);
const expectedByDate = new Map([
  ['2026-11-02', [1]],
  ['2026-11-03', [1, 3]],
  ['2026-11-05', [2, 3]],
  ['2026-11-06', [2]],
]);

function makeCalendar() {
  const days = [];
  for (let day = 1; day <= daysInMonth(MONTH); day += 1) {
    const date = isoDay(day);
    const sessions = classSessions.get(date);
    if (sessions) {
      days.push({ date, kind: 'CLASS_DAY', scheduledSessions: sessions, reasonCode: null, reasonText: null });
    } else if (date === '2026-11-04') {
      days.push({ date, kind: 'NON_CLASS_DAY', scheduledSessions: [], reasonCode: 'HOLIDAY', reasonText: 'Declared school holiday' });
    } else {
      const weekday = new Date(`${date}T00:00:00Z`).getUTCDay();
      days.push({
        date,
        kind: 'NON_CLASS_DAY',
        scheduledSessions: [],
        reasonCode: weekday === 0 || weekday === 6 ? 'WEEKEND' : 'NO_SCHEDULE',
        reasonText: weekday === 0 || weekday === 6 ? 'Weekend' : 'No scheduled class in this synthetic fixture',
      });
    }
  }
  return days;
}

function attendanceEvent(seq, type, status) {
  return { seq, type, status };
}

function absence() {
  return { status: 'ABSENT_CONFIRMED', sourceEvent: null };
}

function committed(status, seq, type = 'SCAN') {
  return { status, sourceEvent: attendanceEvent(seq, type, status) };
}

function attendanceRows() {
  return [
    {
      studentId: 1,
      days: [
        { date: '2026-11-02', am: committed('PRESENT', 1), pm: committed('PRESENT', 2) },
        { date: '2026-11-03', am: committed('LATE', 3), pm: absence() },
      ],
    },
    {
      studentId: 2,
      days: [
        { date: '2026-11-05', am: committed('EXCUSED', 4, 'MARK'), pm: absence() },
        { date: '2026-11-06', am: committed('PRESENT', 5), pm: committed('PRESENT', 6) },
      ],
    },
    {
      studentId: 3,
      days: [
        { date: '2026-11-03', am: absence(), pm: absence() },
        { date: '2026-11-05', am: committed('PRESENT', 7), pm: absence() },
      ],
    },
  ];
}

function makeCoverage() {
  const entries = [];
  let closeEventSeq = 8;
  for (const [date, sessions] of classSessions) {
    for (const session of sessions) {
      const bitmapPayloadSeqs = Array.from({ length: 5 }, (_, index) => closeEventSeq + index + 1);
      const sourceLastSeq = bitmapPayloadSeqs.at(-1);
      entries.push({
        date,
        session,
        sessionClosed: true,
        coverageComplete: true,
        closeCoverageFlag: true,
        logHealthy: true,
        gapAffected: false,
        closeEventSeq,
        bitmapPayloadSeqs,
        sourceLastSeq,
        expectedStudentIds: [...expectedByDate.get(date)],
        expectedCount: expectedByDate.get(date).length,
      });
      closeEventSeq = sourceLastSeq + 1;
    }
  }
  return entries;
}

export function makeValidSnapshot() {
  return {
    schemaVersion: 1,
    snapshotId: 'snapshot-fixture-2026-11-r1',
    month: MONTH,
    sourceRevision: {
      attendanceLastSeq: 55,
      rosterRevision: 'roster-r12',
      configGeneration: 3,
      calendarRevision: 'calendar-r4',
      reportingProfileRevision: 'reporting-rp2',
      schoolProfileRevision: 'school-sp1',
      learnerMetadataRevision: 'learner-meta-r3',
    },
    reportingProfile: {
      profileId: PROFILE_ID,
      revision: 'reporting-rp2',
      curriculum: 'REGULAR_SHS',
      schoolYear: '2026-2027',
      gradeLevel: '11',
      sectionName: '11-A',
      track: null,
      strand: null,
      course: null,
      semesterLabel: null,
      effectiveFrom: '2026-06-01',
      effectiveToExclusive: null,
    },
    schoolProfile: {
      revision: 'school-sp1',
      schoolName: 'Example Senior High School',
      schoolId: '123456',
      district: 'Example District',
      division: 'Example Division',
      region: 'Region VIII',
      adviserName: 'Example Adviser',
      schoolHeadName: 'Example School Head',
    },
    roster: { revision: 'roster-r12' },
    learnerMetadata: { revision: 'learner-meta-r3' },
    calendar: {
      revision: 'calendar-r4',
      configGeneration: 3,
      days: makeCalendar(),
    },
    learners: [
      {
        studentId: 1,
        sex: 'M',
        identity: {
          firstName: 'Adrian', middleName: 'M.', lastName: 'Aldana', extensionName: null, lrn: '123456789012',
        },
        enrollmentPeriods: [
          { profileId: PROFILE_ID, startInclusive: '2026-10-01', endExclusive: '2026-11-04' },
        ],
      },
      {
        studentId: 2,
        sex: 'F',
        identity: {
          firstName: 'Bea', middleName: null, lastName: 'Bautista', extensionName: null, lrn: '223456789012',
        },
        enrollmentPeriods: [
          { profileId: PROFILE_ID, startInclusive: '2026-11-05', endExclusive: null },
        ],
      },
      {
        studentId: 3,
        sex: 'F',
        identity: {
          firstName: 'Carla', middleName: 'D.', lastName: 'Cruz', extensionName: null, lrn: '323456789012',
        },
        enrollmentPeriods: [
          { profileId: PROFILE_ID, startInclusive: '2026-11-03', endExclusive: '2026-11-06' },
        ],
      },
    ],
    attendance: attendanceRows(),
    coverage: makeCoverage(),
    movementHistory: [
      {
        studentId: 1,
        date: '2026-11-04',
        type: 'TRANSFER_OUT',
        effectiveDateMeaning: 'FIRST_NOT_ENROLLED_DAY',
        reasonCode: 'TRANSFERRED_OUT',
        reasonText: 'Transferred to another school',
        destinationSchool: 'Other Senior High School',
        destinationSection: null,
      },
      {
        studentId: 3,
        date: '2026-11-06',
        type: 'TRANSFER_OUT',
        effectiveDateMeaning: 'FIRST_NOT_ENROLLED_DAY',
        reasonCode: 'TRANSFERRED_OUT',
        reasonText: 'Transferred to another school',
        destinationSchool: 'Another Senior High School',
        destinationSection: null,
      },
    ],
    completeness: {
      rosterHistoryComplete: true,
      calendarComplete: true,
      scheduleHistoryComplete: true,
      attendanceSnapshotConsistent: true,
    },
  };
}

export function cloneSnapshot(snapshot = makeValidSnapshot()) {
  return structuredClone(snapshot);
}

export const fixtureExpectations = Object.freeze({
  month: MONTH,
  rosterIds: [1, 2, 3],
  reportLearnerCount: 3,
  classDates: ['2026-11-02', '2026-11-03', '2026-11-05', '2026-11-06'],
  dailyCodes: {
    1: { '2026-11-02': 'BLANK', '2026-11-03': 'AM_ONLY' },
    2: { '2026-11-05': 'E', '2026-11-06': 'BLANK' },
    3: { '2026-11-03': 'X', '2026-11-05': 'AM_ONLY' },
  },
});
