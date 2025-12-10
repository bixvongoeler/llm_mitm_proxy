"""
SIS Advisor - Simplified academic advisor service.

Single class handling all LLM interactions with direct LLMProxy calls.
Stateless backend - widget manages session ID and transcript summary.
"""

import logging
import re
from pathlib import Path
from typing import Any

import pdfplumber

from bs4 import BeautifulSoup

from llmproxy import LLMProxy

logger = logging.getLogger("sis_advisor")

# Paths to context files
CONTEXT_DIR = Path(__file__).parent.parent.parent / "context"
ALL_COURSES_PATH = CONTEXT_DIR / "all_courses.md"
MAJOR_REQS_PATH = CONTEXT_DIR / "cs_major_reqs.md"

# System prompt for the academic advisor
SYSTEM_PROMPT = """You are an AI academic advisor assistant for Tufts University students using the Student Information System (SIS).

Your capabilities:
- Help students search for courses and understand course offerings
- Explain prerequisites, corequisites, and course requirements
- Provide information about degree requirements and distribution credits
- Answer questions about academic policies and procedures
- Guide students through the SIS interface

Guidelines:
- Be concise but helpful - students are busy
- Reference specific course numbers (e.g., CS 112, MATH 42) when relevant
- If you're unsure about specific policies, recommend the student contact their academic advisor
- Never make up course information - only reference what you know
- Be encouraging and supportive of students' academic goals
- If the student asks about something not related to academics, politely redirect

The current page context will be provided with each message to help you understand what the student is looking at."""

# Prompt for summarizing transcripts
TRANSCRIPT_SUMMARY_PROMPT = """You are analyzing a student academic transcript from Tufts University.

Extract ALL of the following information in a structured format. If any field is not found, indicate "Not listed".

## REQUIRED FIELDS:

1. **STUDENT INFORMATION**
   - Full Name
   - Student ID
   - Academic Year/Class Level (Freshman, Sophomore, Junior, Senior)
   - Major(s) and Minor(s)
   - Academic Advisor Name

2. **ACADEMIC STANDING**
   - Current Standing (Good Standing, Dean's List, Probation, etc.)
   - Cumulative GPA
   - Total Credits Earned
   - Credits In Progress

3. **TRANSFER & TEST CREDITS**
   - AP/IB Credits (list each exam and credits awarded)
   - Transfer Credits (institution and credits)
   - Pre-matriculation credits

4. **COMPLETE COURSE HISTORY**
   For EACH semester, list ALL courses in this format:
   - Semester (e.g., "Fall 2023")
   - Course Code | Course Title | Credits | Grade

   List semesters from most recent to oldest.

5. **NOTABLE ITEMS**
   - Courses with W (Withdrawal)
   - Courses with Incomplete
   - Repeated courses
   - Academic honors or awards

Be thorough and include EVERY course. This summary will be used throughout the advising session."""

# Welcome message shown when chat opens
WELCOME_MESSAGE = """Welcome to the Tufts SIS Academic Advisor!

To provide personalized academic guidance, please **upload your transcript** using the button below.

Once uploaded, I can help you with:
- Selecting courses for next semester
- Understanding degree requirements
- Finding prerequisites for courses you're interested in
- Planning your academic path"""

# Suggested questions shown after transcript upload
SUGGESTED_QUESTIONS = [
    "Help me pick courses for this semester",
    "Summarize my current academic history",
    "What degree requirements do I still have",
]


class SISAdvisor:
    """Simple academic advisor using LLMProxy.

    Backend is stateless - widget manages:
    - session_id in localStorage (persists until chat cleared)
    - transcript_summary in localStorage (sent with each chat request)
    """

    def __init__(self) -> None:
        self._client: LLMProxy | None = None
        self._major_reqs: str | None = None
        self._all_courses: str | None = None

    @property
    def client(self) -> LLMProxy:
        """Lazy-load the LLMProxy client."""
        if self._client is None:
            self._client = LLMProxy()
        return self._client

    @property
    def major_reqs(self) -> str:
        """Load CS major requirements from file (cached)."""
        if self._major_reqs is None:
            if MAJOR_REQS_PATH.exists():
                self._major_reqs = MAJOR_REQS_PATH.read_text()
            else:
                self._major_reqs = ""
                logger.warning(f"Major requirements file not found: {MAJOR_REQS_PATH}")
        return self._major_reqs

    @property
    def all_courses(self) -> str:
        """Load all courses content from file (cached)."""
        if self._all_courses is None:
            if ALL_COURSES_PATH.exists():
                self._all_courses = ALL_COURSES_PATH.read_text()
            else:
                self._all_courses = ""
                logger.warning(f"All courses file not found: {ALL_COURSES_PATH}")
        return self._all_courses

    def initialize_session(self, session_id: str) -> dict[str, Any]:
        """
        Initialize a new session by uploading course content to RAG.

        Called by widget when creating a new session ID.

        Args:
            session_id: The new session ID to initialize

        Returns:
            Dict with success status or error
        """
        logger.info(f"Initializing session: {session_id}")

        if not self.all_courses:
            return {"error": "Course content not available"}

        try:
            result = self.client.upload_text(
                text=self.all_courses,
                session_id=session_id,
                description="Tufts CS course catalog and summaries",
            )

            if "error" in result:
                logger.error(f"Failed to upload course content: {result['error']}")
                return {"error": result["error"]}

            logger.info(f"Session initialized successfully: {session_id}")
            return {"success": True}

        except Exception as e:
            logger.exception(f"Exception during session init: {e}")
            return {"error": str(e)}

    def upload_transcript(self, file_path: Path, session_id: str) -> dict[str, Any]:
        """
        Upload transcript PDF and generate summary.

        Uses pdfplumber to extract text from PDF, then summarizes the content.

        Args:
            file_path: Path to PDF file
            session_id: User's session ID

        Returns:
            Dict with success status and summary, or error
        """
        logger.info(f"Uploading transcript: {file_path.name}, session: {session_id}")

        # Step 1: Extract text from PDF using pdfplumber
        try:
            logger.info("Extracting text from PDF with pdfplumber...")
            text_parts = []
            with pdfplumber.open(str(file_path)) as pdf:
                for page_num, page in enumerate(pdf.pages, 1):
                    page_text = page.extract_text()
                    if page_text and page_text.strip():
                        text_parts.append(f"--- Page {page_num} ---\n{page_text}")
            transcript_text = "\n\n".join(text_parts)
            logger.info(f"PDF text extracted, length: {len(transcript_text)}")
        except Exception as e:
            logger.exception(f"pdfplumber extraction failed: {e}")
            return {"error": f"Failed to process PDF: {str(e)}"}

        # Step 2: Upload text to RAG for later context retrieval
        try:
            upload_result = self.client.upload_text(
                text=transcript_text,
                session_id=session_id,
                description="Student academic transcript (extracted from PDF)",
            )

            if "error" in upload_result:
                logger.warning(f"Failed to upload transcript to RAG: {upload_result['error']}")
                # Continue anyway - we have the text for summarization

        except Exception as e:
            logger.warning(f"Exception uploading transcript to RAG: {e}")
            # Continue anyway - we have the text for summarization

        # Step 3: Generate summary by passing text directly to LLM
        try:
            # Truncate if extremely long (shouldn't happen with transcripts)
            content_for_summary = transcript_text
            if len(content_for_summary) > 50000:
                content_for_summary = (
                    content_for_summary[:50000] + "\n\n[Content truncated...]"
                )

            summary_result = self.client.generate(
                model="gpt-4.1-mini",
                system=TRANSCRIPT_SUMMARY_PROMPT,
                query=f"Please analyze and summarize the following academic transcript:\n\n{content_for_summary}",
                temperature=0.3,
                session_id=session_id,
                lastk=0,
                rag_usage=False,  # Don't use RAG - we're passing content directly
            )

            if "error" in summary_result:
                logger.error(f"Failed to summarize transcript: {summary_result['error']}")
                return {"error": summary_result["error"]}

            summary = summary_result.get("result", "")
            logger.info(f"Transcript summarized, length: {len(summary)}")

            return {
                "success": True,
                "message": "Transcript uploaded and analyzed!",
                "summary": summary,
            }

        except Exception as e:
            logger.exception(f"Exception summarizing transcript: {e}")
            return {"error": str(e)}

    def chat(
        self,
        message: str,
        session_id: str,
        page_content: str | None = None,
        transcript_summary: str | None = None,
    ) -> dict[str, Any]:
        """
        Handle chat message.

        Args:
            message: User's message
            session_id: User's session ID
            page_content: Raw HTML from current SIS page (parsed here)
            transcript_summary: Transcript summary from localStorage

        Returns:
            Dict with response or error
        """
        logger.info(f"Chat request - session: {session_id}, message length: {len(message)}")

        # Build system prompt with transcript and major requirements
        system = self._build_system_prompt(transcript_summary)

        # Build query with parsed page content
        query = self._build_query(message, page_content)

        # Generate response with RAG for course context
        try:
            result = self.client.generate(
                model="gpt-4.1-mini",
                system=system,
                query=query,
                temperature=0.7,
                session_id=session_id,
                lastk=10,
                rag_usage=True,
                rag_threshold=0.3,
                rag_k=10,
            )

            if "error" in result:
                logger.error(f"Generate error: {result['error']}")
                return {"error": result["error"]}

            response = result.get("result", "I couldn't generate a response.")
            logger.info(f"Chat response generated, length: {len(response)}")

            return {"response": response}

        except Exception as e:
            logger.exception(f"Exception in chat: {e}")
            return {"error": str(e)}

    def get_welcome_response(self) -> dict[str, Any]:
        """Get welcome message for new users."""
        return {
            "response": WELCOME_MESSAGE,
            "suggested_questions": SUGGESTED_QUESTIONS,
            "requires_transcript": True,
        }

    def parse_page_content(self, html: str) -> str:
        """
        Extract visible text from HTML using BeautifulSoup.

        Special handling for SIS course search results to extract structured data.

        Args:
            html: Raw HTML string

        Returns:
            Cleaned visible text, formatted for course search results
        """
        if not html:
            return ""

        try:
            soup = BeautifulSoup(html, "html.parser")

            # Check if this is a course search results page
            search_results = soup.find(id="TFP_CLSSRCH_accordion")
            if search_results:
                result = self._parse_course_search_results(soup)
                self._debug_log_parsed_content(result, "course_search")
                return result

            # Fallback: generic text extraction
            # Remove non-content elements
            for element in soup.find_all(
                [
                    "script",
                    "style",
                    "noscript",
                    "header",
                    "footer",
                    "nav",
                    "meta",
                    "link",
                    "iframe",
                ]
            ):
                element.decompose()

            # Remove hidden elements
            for element in soup.find_all(style=lambda s: bool(s and "display:none" in s)):
                element.decompose()
            for element in soup.find_all(
                class_=lambda c: bool(c and "hidden" in str(c).lower())
            ):
                element.decompose()

            # Get text and clean whitespace
            text = soup.get_text(separator=" ", strip=True)

            # Collapse multiple spaces/newlines
            text = re.sub(r"\s+", " ", text)

            result = text.strip()
            self._debug_log_parsed_content(result, "generic")
            return result

        except Exception as e:
            logger.warning(f"HTML parsing failed: {e}")
            return html[:3000] if len(html) > 3000 else html

    def _parse_course_search_results(self, soup: BeautifulSoup) -> str:
        """
        Parse SIS course search results into structured text.

        Extracts course codes, titles, descriptions, and section details.
        """
        lines = []
        courses_offered = []

        # Get header info (result count, term, subject)
        header = soup.find(id="tfp_searchresultsHeader_region")
        if header:
            title = header.find("h1")
            if title:
                lines.append(f"# {title.get_text(strip=True)}")
            count_head = header.find(class_="tfp-count-head")
            if count_head:
                lines.append(count_head.get_text(strip=True))
            lines.append("")

        # Process each course
        course_rows = soup.find_all(class_="tfp_accordion_row")
        for row in course_rows:
            # Course code and title
            course_head = row.find(class_="accorion-head")
            if course_head:
                # Get text with separator to preserve spacing between elements
                course_text = course_head.get_text(separator=" ", strip=True)
                # Collapse multiple spaces
                course_text = re.sub(r"\s+", " ", course_text)
                lines.append(f"## {course_text}")
                courses_offered.append(course_text)

            # Description
            # desc = row.find(class_="tfp-course-desc")
            # if desc:
            #     desc_text = desc.get_text(strip=True)
            #     if desc_text:
            #         lines.append(f"Description: {desc_text}")

            # Sections
            sections_div = row.find(class_="tfp-sections")
            if sections_div:
                for table in sections_div.find_all("table", class_="tfp-results-sections"):
                    # Section type (Lecture, Lab, Recitation)

                    caption = table.find("caption")
                    if caption:
                        # Skip all sections not of type Lecture:
                        if caption.get_text(strip=True) != "Lecture":
                            continue
                        lines.append(f"  {caption.get_text(strip=True)}:")

                    # Process each section row
                    for tr in table.find_all("tr", class_="accorion-head"):
                        section_info = self._extract_section_info(tr)
                        if section_info:
                            lines.append(f"    {section_info}")

            lines.append("---")  # Blank line between courses

        # join lines appended after courses offered:
        courses_offered_str = "\n".join(courses_offered)
        return f"{courses_offered_str}\n\n{''.join(lines)}"

    def _extract_section_info(self, tr) -> str:
        """Extract section information from a table row."""
        parts = []

        # Get all td cells
        cells = tr.find_all("td")
        if not cells:
            return ""

        # Section ID (first cell)
        if cells:
            section_text = cells[0].get_text(strip=True).replace("Details", "").strip()
            if section_text:
                parts.append(section_text)

        # Class number (second cell)
        if len(cells) > 1:
            class_num = cells[1].get_text(strip=True)
            if class_num:
                parts.append(f"#{class_num}")

        # Day/Time/Location (in tfp-loc div)
        loc_div = tr.find(class_="tfp-loc")
        if loc_div:
            loc_text = " ".join(loc_div.stripped_strings)
            loc_text = re.sub(r"\s+", " ", loc_text)
            if loc_text:
                parts.append(loc_text)

        # Instructor (in tfp-ins div)
        ins_div = tr.find(class_="tfp-ins")
        if ins_div:
            ins_text = ins_div.get_text(strip=True)
            if ins_text:
                parts.append(f"({ins_text})")

        # Credits (typically 6th cell)
        if len(cells) > 5:
            credits = cells[5].get_text(strip=True)
            if credits:
                parts.append(f"{credits} cr")

        # Status (from img alt in 7th cell)
        if len(cells) > 6:
            status_img = cells[6].find("img")
            if status_img and status_img.get("alt"):
                status = status_img["alt"].replace(" status image", "")
                parts.append(f"[{status}]")
            else:
                # Check for "Enrolled" or "In Cart" text
                status_text = cells[6].get_text(strip=True)
                if status_text:
                    parts.append(f"[{status_text}]")

        return " | ".join(parts)

    def _debug_log_parsed_content(self, content: str, parse_type: str) -> None:
        """Write parsed content to debug log file."""
        import datetime
        from pathlib import Path

        log_dir = Path(__file__).parent.parent.parent.parent / "logs" / "parsed_content"
        log_dir.mkdir(parents=True, exist_ok=True)

        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        log_file = log_dir / f"{timestamp}_{parse_type}.txt"

        try:
            with open(log_file, "w") as f:
                f.write(f"Parse type: {parse_type}\n")
                f.write(f"Content length: {len(content)} chars\n")
                f.write("=" * 60 + "\n")
                f.write(content)
            logger.info(f"Parsed content logged to: {log_file}")
        except Exception as e:
            logger.warning(f"Failed to write debug log: {e}")

    def _build_system_prompt(self, transcript_summary: str | None) -> str:
        """Build system prompt with transcript and major requirements."""
        parts = [SYSTEM_PROMPT]

        # Add major requirements
        if self.major_reqs:
            parts.append(f"\n\n[CS Major Requirements]\n{self.major_reqs}")

        # Add transcript summary if provided
        if transcript_summary:
            parts.append(f"\n\n[Student Academic Information]\n{transcript_summary}")

        return "".join(parts)

    def _build_query(self, message: str, page_content: str | None) -> str:
        """Build query with parsed page content."""
        if not page_content:
            return message

        # Parse HTML to extract visible text
        parsed = self.parse_page_content(page_content)

        if not parsed:
            return message

        # Truncate if very long (shouldn't happen often with parsed content)
        if len(parsed) > 10000:
            parsed = (
                parsed[:10000]
                + "... [# PAGE TRUNCATED. INFORM USER THAT FULL PAGE CANNOT BE SEEN]"
            )

        return f"[# Current SIS Page Content]\n{parsed}\n\n[# Student Question]\n{message}"


# Singleton instance
_advisor: SISAdvisor | None = None


def get_advisor() -> SISAdvisor:
    """Get the SIS advisor singleton."""
    global _advisor
    if _advisor is None:
        _advisor = SISAdvisor()
    return _advisor
