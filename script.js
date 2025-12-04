const API_URLS = {
  users: "https://n8n.abdallav2ray.ggff.net/webhook/user",
  sessions: "https://n8n.abdallav2ray.ggff.net/webhook/Sessions",
  attendance: "https://n8n.abdallav2ray.ggff.net/webhook/Attendance",
};

let usersData = [];
let sessionsData = [];
let attendanceData = [];
let allStudentsHTML = "";
let allSessionsHTML = "";

function toggleTheme() {
  const html = document.documentElement;
  const currentTheme = html.getAttribute("data-theme");
  const newTheme = currentTheme === "dark" ? "light" : "dark";
  html.setAttribute("data-theme", newTheme);
  localStorage.setItem("theme", newTheme);

  const icon = document.getElementById("themeIcon");
  const text = document.getElementById("themeText");
  if (newTheme === "light") {
    icon.textContent = "☀️";
    text.textContent = "الوضع الفاتح";
  } else {
    icon.textContent = "🌙";
    text.textContent = "الوضع الداكن";
  }
}

function loadTheme() {
  const savedTheme = localStorage.getItem("theme") || "dark";
  document.documentElement.setAttribute("data-theme", savedTheme);

  const icon = document.getElementById("themeIcon");
  const text = document.getElementById("themeText");
  if (savedTheme === "light") {
    icon.textContent = "☀️";
    text.textContent = "الوضع الفاتح";
  } else {
    icon.textContent = "🌙";
    text.textContent = "الوضع الداكن";
  }
}

function switchTab(tabName) {
  document
    .querySelectorAll(".tab")
    .forEach((t) => t.classList.remove("active"));
  document
    .querySelectorAll(".content-section")
    .forEach((c) => c.classList.remove("active"));

  event.target.closest(".tab").classList.add("active");
  document.getElementById(tabName).classList.add("active");
}

function filterTable(tableId, searchValue) {
  const table = document.getElementById(tableId);
  const rows = table
    .getElementsByTagName("tbody")[0]
    .getElementsByTagName("tr");

  for (let row of rows) {
    const text = row.textContent.toLowerCase();
    row.style.display = text.includes(searchValue.toLowerCase()) ? "" : "none";
  }
}

function filterStudents(searchValue) {
  const container = document.getElementById("studentsContainer");
  const cards = container.getElementsByClassName("student-card");

  for (let card of cards) {
    const text = card.textContent.toLowerCase();
    card.style.display = text.includes(searchValue.toLowerCase()) ? "" : "none";
  }
}

function filterSessions(searchValue) {
  const container = document.getElementById("sessionsContainer");
  const cards = container.getElementsByClassName("session-card");

  for (let card of cards) {
    const text = card.textContent.toLowerCase();
    card.style.display = text.includes(searchValue.toLowerCase()) ? "" : "none";
  }
}

async function fetchData() {
  try {
    const [users, sessions, attendance] = await Promise.all([
      fetch(API_URLS.users).then((r) => r.json()),
      fetch(API_URLS.sessions).then((r) => r.json()),
      fetch(API_URLS.attendance).then((r) => r.json()),
    ]);

    usersData = users;
    sessionsData = sessions;
    attendanceData = attendance;

    updateStats();
    renderAttendanceChart();
    renderOverview();
    renderStudents();
    renderTeachers();
    renderSessions();
    renderAttendance();
  } catch (error) {
    console.error("خطأ في تحميل البيانات:", error);
    alert("حدث خطأ في تحميل البيانات. يرجى المحاولة مرة أخرى.");
  }
}

function updateStats() {
  const students = usersData.filter((u) => u.role === "student");
  const teachers = usersData.filter((u) => u.role === "teacher");

  animateValue("totalStudents", 0, students.length, 1000);
  animateValue("totalTeachers", 0, teachers.length, 1000);
  animateValue("totalSessions", 0, sessionsData.length, 1000);
  animateValue("totalAttendance", 0, attendanceData.length, 1000);
}

function animateValue(id, start, end, duration) {
  const element = document.getElementById(id);
  const range = end - start;
  const increment = range / 50;
  let current = start;
  const timer = setInterval(() => {
    current += increment;
    if (
      (increment > 0 && current >= end) ||
      (increment < 0 && current <= end)
    ) {
      current = end;
      clearInterval(timer);
    }
    element.textContent = Math.round(current);
  }, duration / 50);
}

function renderAttendanceChart() {
  const students = usersData.filter((u) => u.role === "student");
  const chartContainer = document.getElementById("attendanceChart");

  if (students.length === 0) {
    chartContainer.innerHTML = '<div class="loading">لا توجد بيانات</div>';
    return;
  }

  const chartData = students.slice(0, 6).map((student) => {
    let attendanceInfo = { rate: "0%", present: 0, total: 0 };

    if (student["Network theory"]) {
      try {
        const data = JSON.parse(student["Network theory"]);
        attendanceInfo = {
          rate: parseFloat(data.rate) || 0,
          present: data.present || 0,
          total: data.total_sessions || 0,
        };
      } catch (e) {}
    }

    return {
      name: student.name,
      rate: attendanceInfo.rate,
      present: attendanceInfo.present,
      total: attendanceInfo.total,
    };
  });

  const maxRate = Math.max(...chartData.map((d) => d.rate), 100);

  chartContainer.innerHTML = chartData
    .map((data) => {
      const height = Math.max((data.rate / 100) * 240, 20);
      const gradient =
        data.rate >= 75
          ? "var(--gradient-4)"
          : data.rate >= 50
          ? "var(--gradient-3)"
          : "var(--gradient-2)";

      return `
                    <div class="bar-item">
                        <div class="bar-value">${data.rate.toFixed(1)}%</div>
                        <div class="bar-wrapper">
                            <div class="bar" style="height: ${height}px; background: ${gradient}"></div>
                        </div>
                        <div class="bar-label" title="${data.name}">${data.name
        .split(" ")
        .slice(0, 2)
        .join(" ")}</div>
                    </div>
                `;
    })
    .join("");
}

function renderOverview() {
  const students = usersData.filter((u) => u.role === "student");
  let totalPresent = 0;
  let totalAbsent = 0;

  students.forEach((student) => {
    if (student["Network theory"]) {
      try {
        const data = JSON.parse(student["Network theory"]);
        totalPresent += data.present || 0;
        totalAbsent += data.absent || 0;
      } catch (e) {}
    }
  });

  const total = totalPresent + totalAbsent;
  const presentPercentage =
    total > 0 ? ((totalPresent / total) * 100).toFixed(1) : 0;
  const absentPercentage =
    total > 0 ? ((totalAbsent / total) * 100).toFixed(1) : 0;

  const presentAngle = (totalPresent / total) * 360;

  document.getElementById("overviewChart").innerHTML = `
                <div class="pie-visual" style="--present-angle: ${presentAngle}deg"></div>
                <div class="pie-legend">
                    <div class="legend-item">
                        <div class="legend-color" style="background: var(--success)"></div>
                        <div>
                            <div class="legend-text">حاضر: ${totalPresent}</div>
                            <div class="detail-label">${presentPercentage}%</div>
                        </div>
                    </div>
                    <div class="legend-item">
                        <div class="legend-color" style="background: var(--danger)"></div>
                        <div>
                            <div class="legend-text">غائب: ${totalAbsent}</div>
                            <div class="detail-label">${absentPercentage}%</div>
                        </div>
                    </div>
                    <div class="legend-item">
                        <div class="legend-color" style="background: linear-gradient(135deg, var(--success), var(--danger))"></div>
                        <div>
                            <div class="legend-text">الإجمالي: ${total}</div>
                            <div class="detail-label">100%</div>
                        </div>
                    </div>
                </div>
            `;
}

function renderStudents() {
  const students = usersData.filter((u) => u.role === "student");
  const container = document.getElementById("studentsContainer");

  if (students.length === 0) {
    container.innerHTML = '<div class="loading">لا توجد بيانات</div>';
    return;
  }

  allStudentsHTML = students
    .map((student) => {
      let attendanceInfo = {
        rate: "0%",
        present: 0,
        absent: 0,
        total: 0,
        sessions: [],
      };

      if (student["Network theory"]) {
        try {
          const data = JSON.parse(student["Network theory"]);
          attendanceInfo = {
            rate: data.rate || "0%",
            present: data.present || 0,
            absent: data.absent || 0,
            total: data.total_sessions || 0,
            sessions: data.sessions || [],
          };
        } catch (e) {}
      }

      const rateValue = parseFloat(attendanceInfo.rate);
      const badgeClass =
        rateValue >= 75
          ? "badge-success"
          : rateValue >= 50
          ? "badge-warning"
          : "badge-danger";
      const statusText =
        rateValue >= 75 ? "🌟 ممتاز" : rateValue >= 50 ? "⚠️ جيد" : "❌ ضعيف";

      return `
                    <div class="student-card">
                        <div class="student-header">
                            <div class="student-name">${student.name}</div>
                            <span class="badge ${badgeClass}">${statusText}</span>
                        </div>
                        <div class="student-details">
                            <div class="detail-item">
                                <div class="detail-label">رقم WhatsApp</div>
                                <div class="detail-value">📱 ${
                                  student.whatsapp || "-"
                                }</div>
                            </div>
                            <div class="detail-item">
                                <div class="detail-label">نسبة الحضور</div>
                                <div class="detail-value">${
                                  attendanceInfo.rate
                                }</div>
                            </div>
                            <div class="detail-item">
                                <div class="detail-label">الحضور</div>
                                <div class="detail-value" style="color: var(--success)">✅ ${
                                  attendanceInfo.present
                                }</div>
                            </div>
                            <div class="detail-item">
                                <div class="detail-label">الغياب</div>
                                <div class="detail-value" style="color: var(--danger)">❌ ${
                                  attendanceInfo.absent
                                }</div>
                            </div>
                            <div class="detail-item">
                                <div class="detail-label">إجمالي المحاضرات</div>
                                <div class="detail-value">📚 ${
                                  attendanceInfo.total
                                }</div>
                            </div>
                            <div class="detail-item">
                                <div class="detail-label">RFID</div>
                                <div class="detail-value">${
                                  student.uid_rfid
                                }</div>
                            </div>
                        </div>
                        <div class="progress-bar">
                            <div class="progress-fill" style="width: ${rateValue}%; background: ${
        rateValue >= 75
          ? "var(--gradient-4)"
          : rateValue >= 50
          ? "var(--gradient-3)"
          : "var(--gradient-2)"
      }"></div>
                        </div>
                    </div>
                `;
    })
    .join("");

  container.innerHTML = allStudentsHTML;
}

function renderTeachers() {
  const teachers = usersData.filter((u) => u.role === "teacher");
  const tbody = document.getElementById("teachersBody");

  if (teachers.length === 0) {
    tbody.innerHTML =
      '<tr><td colspan="5" class="loading">لا توجد بيانات</td></tr>';
    return;
  }

  tbody.innerHTML = teachers
    .map((teacher, index) => {
      const teacherSessions = sessionsData.filter(
        (s) =>
          s.teacher_uid === teacher.uid_rfid || s.teacher_name === teacher.name
      );

      return `
                    <tr>
                        <td><strong>${index + 1}</strong></td>
                        <td><strong>${teacher.name}</strong></td>
                        <td><span class="badge badge-info">📱 ${
                          teacher.whatsapp || "-"
                        }</span></td>
                        <td><span class="badge badge-purple">${
                          teacher.uid_rfid
                        }</span></td>
                        <td><span class="badge badge-success">📅 ${
                          teacherSessions.length
                        } محاضرة</span></td>
                    </tr>
                `;
    })
    .join("");
}

function renderSessions() {
  const container = document.getElementById("sessionsContainer");

  if (sessionsData.length === 0) {
    container.innerHTML = '<div class="loading">لا توجد بيانات</div>';
    return;
  }

  // ⭐ اعكس المحاضرات هنا
  allSessionsHTML = [...sessionsData]
    .reverse()
    .map((session, index) => {
      const attendanceCount = attendanceData.filter(
        (a) => a.session_id === session.session_id
      ).length;

      const statusBadge =
        session.status === "Closed" ? "badge-danger" : "badge-success";
      const statusIcon = session.status === "Closed" ? "🔒" : "🔓";
      const statusText = session.status === "Closed" ? "مغلقة" : "مفتوحة";

      const startDate = new Date(session.start_time);
      const endDate = new Date(session.end_time);
      const duration = Math.round((endDate - startDate) / 1000 / 60);

      return `
        <div class="session-card">
            <div class="student-header">
                <div class="student-name">📝 ${session.session_id}</div>
                <span class="badge ${statusBadge}">${statusIcon} ${statusText}</span>
            </div>
            <div class="session-info">
                <div class="detail-item">
                    <div class="detail-label">الدكتور</div>
                    <div class="detail-value">👨‍🏫 ${session.teacher_name}</div>
                </div>
                <div class="detail-item">
                    <div class="detail-label">وقت البداية</div>
                    <div class="detail-value">🕐 ${session.start_time}</div>
                </div>
                <div class="detail-item">
                    <div class="detail-label">وقت النهاية</div>
                    <div class="detail-value">🕐 ${session.end_time}</div>
                </div>
                <div class="detail-item">
                    <div class="detail-label">المدة</div>
                    <div class="detail-value">⏱️ ${duration} دقيقة</div>
                </div>
                <div class="detail-item">
                    <div class="detail-label">عدد الحضور</div>
                    <div class="detail-value" style="color: var(--success)">👥 ${attendanceCount} طالب</div>
                </div>
            </div>
        </div>
      `;
    })
    .join("");

  container.innerHTML = allSessionsHTML;
}


function renderAttendance() {
  const tbody = document.getElementById("attendanceBody");

  if (attendanceData.length === 0) {
    tbody.innerHTML =
      '<tr><td colspan="6" class="loading">لا توجد بيانات</td></tr>';
    return;
  }

  tbody.innerHTML = attendanceData
    .map(
      (record, index) => `
                <tr>
                    <td><strong>${index + 1}</strong></td>
                    <td>${record.session_id}</td>
                    <td><strong>${record.student_name}</strong></td>
                    <td>${record.teacher_name}</td>
                    <td>${record.scan_time}</td>
                    <td><span class="badge badge-success">✅ ${
                      record.status === "Present" ? "حاضر" : "غائب"
                    }</span></td>
                </tr>
            `
    )
    .join("");
}

loadTheme();
fetchData();
