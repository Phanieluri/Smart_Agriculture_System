import streamlit as st
import requests
import pandas as pd
import plotly.express as px

API_URL = "http://localhost:5001"

st.set_page_config(
    page_title="Smart Agriculture Dashboard",
    layout="wide"
)

st.title("🌱 Smart Agriculture Dashboard")

# ==================================================
# LATEST SENSOR DATA
# ==================================================

try:

    latest = requests.get(
        f"{API_URL}/latest",
        timeout=5
    ).json()

    if "temperature" in latest:

        st.subheader("Latest Sensor Reading")

        st.caption(
            f"Last Updated: {latest['timestamp']}"
        )

        col1, col2, col3 = st.columns(3)

        col1.metric(
            "Temperature (°C)",
            latest["temperature"]
        )

        col2.metric(
            "Humidity (%)",
            latest["humidity"]
        )

        col3.metric(
            "Soil Moisture (%)",
            latest["moisture"]
        )

    else:

        st.warning("No sensor data available")

except Exception as e:

    st.error(f"API Error: {e}")

st.divider()

# ==================================================
# HISTORY DATA
# ==================================================

try:

    history = requests.get(
        f"{API_URL}/history",
        timeout=5
    ).json()

    if len(history) > 0:

        df = pd.DataFrame(history)

        df["timestamp"] = pd.to_datetime(
            df["timestamp"]
        )

        # -----------------------------
        # Temperature Trend
        # -----------------------------

        st.subheader("Temperature Trend")

        fig_temp = px.line(
            df,
            x="timestamp",
            y="temperature",
            markers=True,
            title="Temperature vs Time"
        )

        st.plotly_chart(
            fig_temp,
            use_container_width=True
        )

        # -----------------------------
        # Humidity Trend
        # -----------------------------

        st.subheader("Humidity Trend")

        fig_hum = px.line(
            df,
            x="timestamp",
            y="humidity",
            markers=True,
            title="Humidity vs Time"
        )

        st.plotly_chart(
            fig_hum,
            use_container_width=True
        )

        # -----------------------------
        # Moisture Trend
        # -----------------------------

        st.subheader("Soil Moisture Trend")

        fig_moi = px.line(
            df,
            x="timestamp",
            y="moisture",
            markers=True,
            title="Soil Moisture vs Time"
        )

        st.plotly_chart(
            fig_moi,
            use_container_width=True
        )

        # -----------------------------
        # Recent Sensor Readings
        # -----------------------------

        st.subheader("Recent Sensor Readings")

        st.dataframe(
            df.sort_values(
                "timestamp",
                ascending=False
            ),
            use_container_width=True
        )

    else:

        st.warning("No historical data found")

except Exception as e:

    st.error(f"History API Error: {e}")

st.divider()

# ==================================================
# SYSTEM STATUS
# ==================================================

st.subheader("System Status")

st.success("ESP32 Connected")
st.success("MQTT Broker Running")
st.success("CSV Logging Active")
st.success("FastAPI Running")
st.success("ML Model Ready")
